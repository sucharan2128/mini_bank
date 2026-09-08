import hashlib
import json
import os
import secrets
import sqlite3
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

HOST = "127.0.0.1"
PORT = 8000
DATABASE = os.path.join(os.path.dirname(__file__), "bank_api.sqlite3")
OTP_STORE = {}
MAX_ACCOUNTS = 50


def get_connection():
    connection = sqlite3.connect(DATABASE)
    connection.row_factory = sqlite3.Row
    return connection


def initialize_database():
    with get_connection() as connection:
        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS accounts (
                account_number INTEGER PRIMARY KEY,
                owner_name TEXT NOT NULL,
                phone_number TEXT NOT NULL UNIQUE,
                pin_hash TEXT NOT NULL,
                balance REAL NOT NULL DEFAULT 0 CHECK (balance >= 0),
                is_active INTEGER NOT NULL DEFAULT 1
            )
            """
        )


def hash_pin(pin):
    return hashlib.sha256(pin.encode("utf-8")).hexdigest()


def valid_pin(pin):
    return isinstance(pin, str) and pin.isdigit() and len(pin) == 4


def valid_phone(phone):
    return isinstance(phone, str) and phone.isdigit() and len(phone) == 10


def json_account(row, include_balance=True):
    account = {
        "accountNumber": row["account_number"],
        "ownerName": row["owner_name"],
        "phoneNumber": row["phone_number"],
    }
    if include_balance:
        account["balance"] = round(row["balance"], 2)
    return account


class BankApiHandler(BaseHTTPRequestHandler):
    def send_json(self, status, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def read_json(self):
        length = int(self.headers.get("Content-Length", 0))
        if length == 0:
            return {}
        return json.loads(self.rfile.read(length).decode("utf-8"))

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/api/health":
            self.send_json(200, {"status": "ok"})
            return

        if path == "/api/accounts":
            with get_connection() as connection:
                rows = connection.execute(
                    "SELECT account_number, owner_name, phone_number, balance FROM accounts WHERE is_active = 1 ORDER BY account_number"
                ).fetchall()
            self.send_json(200, {"accounts": [json_account(row) for row in rows]})
            return

        parts = path.strip("/").split("/")
        if len(parts) == 3 and parts[0] == "api" and parts[1] == "accounts":
            try:
                account_number = int(parts[2])
            except ValueError:
                self.send_json(400, {"error": "Invalid account number"})
                return

            with get_connection() as connection:
                row = connection.execute(
                    "SELECT account_number, owner_name, phone_number, balance FROM accounts WHERE account_number = ? AND is_active = 1",
                    (account_number,),
                ).fetchone()
            if row is None:
                self.send_json(404, {"error": "Account not found"})
            else:
                self.send_json(200, json_account(row))
            return

        self.send_json(404, {"error": "Endpoint not found"})

    def do_DELETE(self):
        path = urlparse(self.path).path
        parts = path.strip("/").split("/")
        if len(parts) != 3 or parts[0] != "api" or parts[1] != "accounts":
            self.send_json(404, {"error": "Endpoint not found"})
            return

        try:
            account_number = int(parts[2])
            data = self.read_json()
        except (ValueError, json.JSONDecodeError):
            self.send_json(400, {"error": "Invalid account number or request body"})
            return

        with get_connection() as connection:
            row = self.authenticate(connection, account_number, data.get("pin"))
            if row is None:
                self.send_json(401, {"error": "Invalid account number or PIN"})
                return
            if row["balance"] != 0:
                self.send_json(400, {"error": "Withdraw or transfer the remaining balance before closing the account"})
                return
            connection.execute(
                "UPDATE accounts SET is_active = 0 WHERE account_number = ?",
                (account_number,),
            )

        self.send_json(200, {"message": "Account closed"})

    def do_POST(self):
        path = urlparse(self.path).path
        try:
            data = self.read_json()
        except (ValueError, json.JSONDecodeError):
            self.send_json(400, {"error": "Request body must be valid JSON"})
            return

        if path == "/api/otp":
            phone = str(data.get("phone", ""))
            if not valid_phone(phone):
                self.send_json(400, {"error": "Phone number must contain 10 digits"})
                return
            code = str(secrets.randbelow(9000) + 1000)
            OTP_STORE[phone] = code
            self.send_json(200, {
                "message": "OTP generated for local testing",
                "phone": phone,
                "otp": code,
            })
            return

        if path == "/api/accounts":
            self.create_account(data)
            return

        parts = path.strip("/").split("/")
        if len(parts) == 4 and parts[0] == "api" and parts[1] == "accounts":
            try:
                account_number = int(parts[2])
            except ValueError:
                self.send_json(400, {"error": "Invalid account number"})
                return

            action = parts[3]
            if action == "deposit":
                self.change_balance(account_number, data, 1)
            elif action == "withdraw":
                self.change_balance(account_number, data, -1)
            elif action == "transfer":
                self.transfer(account_number, data)
            elif action == "balance":
                self.balance_account(account_number, data)
            else:
                self.send_json(404, {"error": "Endpoint not found"})
            return

        self.send_json(404, {"error": "Endpoint not found"})

    def create_account(self, data):
        name = str(data.get("ownerName", "")).strip()
        phone = str(data.get("phoneNumber", ""))
        pin = str(data.get("pin", ""))
        otp = str(data.get("otp", ""))
        try:
            initial_balance = float(data.get("initialDeposit", 0))
        except (TypeError, ValueError):
            self.send_json(400, {"error": "Initial deposit must be a number"})
            return

        if not name or not valid_phone(phone) or not valid_pin(pin):
            self.send_json(400, {"error": "Name, 10-digit phone number, and 4-digit PIN are required"})
            return
        if initial_balance < 0:
            self.send_json(400, {"error": "Initial deposit cannot be negative"})
            return
        if OTP_STORE.get(phone) != otp:
            self.send_json(401, {"error": "Invalid or expired OTP"})
            return

        with get_connection() as connection:
            connection.execute("BEGIN IMMEDIATE")
            count = connection.execute("SELECT COUNT(*) FROM accounts WHERE is_active = 1").fetchone()[0]
            if count >= MAX_ACCOUNTS:
                self.send_json(409, {"error": "Bank account limit of 50 reached"})
                return
            next_number = connection.execute(
                "SELECT COALESCE(MAX(account_number), 1000) + 1 FROM accounts"
            ).fetchone()[0]
            try:
                connection.execute(
                    "INSERT INTO accounts(account_number, owner_name, phone_number, pin_hash, balance) VALUES (?, ?, ?, ?, ?)",
                    (next_number, name, phone, hash_pin(pin), initial_balance),
                )
            except sqlite3.IntegrityError:
                self.send_json(409, {"error": "Phone number is already registered"})
                return

        OTP_STORE.pop(phone, None)
        self.send_json(201, {"message": "Account created", "accountNumber": next_number})

    def authenticate(self, connection, account_number, pin):
        row = connection.execute(
            "SELECT * FROM accounts WHERE account_number = ? AND is_active = 1",
            (account_number,),
        ).fetchone()
        if row is None or not valid_pin(str(pin)) or row["pin_hash"] != hash_pin(str(pin)):
            return None
        return row

    def balance_account(self, account_number, data):
        with get_connection() as connection:
            row = self.authenticate(connection, account_number, data.get("pin"))
            if row is None:
                self.send_json(401, {"error": "Invalid account number or PIN"})
                return
            self.send_json(200, {
                "accountNumber": row["account_number"],
                "ownerName": row["owner_name"],
                "balance": round(row["balance"], 2),
            })

    def change_balance(self, account_number, data, direction):
        try:
            amount = float(data.get("amount"))
        except (TypeError, ValueError):
            self.send_json(400, {"error": "Amount must be a number"})
            return
        if amount <= 0:
            self.send_json(400, {"error": "Amount must be positive"})
            return

        with get_connection() as connection:
            row = self.authenticate(connection, account_number, data.get("pin"))
            if row is None:
                self.send_json(401, {"error": "Invalid account number or PIN"})
                return
            new_balance = row["balance"] + direction * amount
            if new_balance < 0:
                self.send_json(400, {"error": "Insufficient funds"})
                return
            connection.execute("UPDATE accounts SET balance = ? WHERE account_number = ?", (new_balance, account_number))
        self.send_json(200, {"message": "Transaction successful", "balance": round(new_balance, 2)})

    def transfer(self, from_number, data):
        try:
            to_number = int(data.get("toAccount"))
            amount = float(data.get("amount"))
        except (TypeError, ValueError):
            self.send_json(400, {"error": "Recipient account and amount are required"})
            return
        if amount <= 0 or from_number == to_number:
            self.send_json(400, {"error": "Invalid transfer"})
            return

        with get_connection() as connection:
            sender = self.authenticate(connection, from_number, data.get("pin"))
            recipient = connection.execute(
                "SELECT * FROM accounts WHERE account_number = ? AND is_active = 1", (to_number,)
            ).fetchone()
            if sender is None or recipient is None:
                self.send_json(401, {"error": "Invalid account, PIN, or recipient"})
                return
            if sender["balance"] < amount:
                self.send_json(400, {"error": "Insufficient funds"})
                return
            connection.execute("UPDATE accounts SET balance = balance - ? WHERE account_number = ?", (amount, from_number))
            connection.execute("UPDATE accounts SET balance = balance + ? WHERE account_number = ?", (amount, to_number))
        self.send_json(200, {"message": "Transfer successful", "balance": round(sender["balance"] - amount, 2)})


if __name__ == "__main__":
    initialize_database()
    server = ThreadingHTTPServer((HOST, PORT), BankApiHandler)
    print(f"Mini Bank API running at http://{HOST}:{PORT}")
    print("Press Ctrl+C to stop the server.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
    finally:
        server.server_close()
