# Mini Bank API

`api.py` provides a local REST API backed by SQLite. It uses only Python's standard library, so no extra package is required.

## Start

```powershell
python api.py
```

The server runs at `http://127.0.0.1:8000` and stores data in `bank_api.sqlite3`.

## Endpoints

### Health check

```http
GET /api/health
```

### Generate OTP

```http
POST /api/otp
Content-Type: application/json

{"phone":"9876543210"}
```

The returned OTP is for local testing. A real service would send it through an SMS provider.

### Create account

```http
POST /api/accounts
Content-Type: application/json

{
  "ownerName": "Alice Johnson",
  "phoneNumber": "9876543210",
  "pin": "1234",
  "otp": "1234",
  "initialDeposit": 5000
}
```

### List active accounts

```http
GET /api/accounts
```

### Get one account

```http
GET /api/accounts/1001
```

### Close an account

An account must have a zero balance before it can be closed. Closing an account
marks it inactive, so it no longer appears in active account lists.

```http
DELETE /api/accounts/1001
Content-Type: application/json

{"pin":"1234"}
```

### Deposit

```http
POST /api/accounts/1001/deposit
Content-Type: application/json

{"pin":"1234","amount":500}
```

### Withdraw

```http
POST /api/accounts/1001/withdraw
Content-Type: application/json

{"pin":"1234","amount":250}
```

### Transfer

```http
POST /api/accounts/1001/transfer
Content-Type: application/json

{"pin":"1234","toAccount":1002,"amount":100}
```

The API enforces a maximum of 50 active accounts and hashes PINs before storing them. It is intended for local coursework; production banking requires HTTPS, rate limiting, authenticated admin access, secure SMS OTP delivery, audit logs, and a real secrets-management system.
