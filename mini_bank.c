/*
 * ============================================================
 *   MINI BANK / ATM SYSTEM — C Course Project
 * ============================================================
 *
 *  CONCEPTS YOU WILL PRACTISE:
 *    - Variables & data types  (int, float, char arrays)
 *    - Functions               (modular design)
 *    - Loops                   (while / for)
 *    - Conditionals            (if / else, switch)
 *    - Structs                 (grouping related data)
 *    - Arrays of structs       (storing multiple accounts)
 *    - String handling         (strcmp, strcpy, strlen)
 *
 *  HOW TO COMPILE & RUN:
 *    gcc mini_bank.c -o mini_bank
 *    ./mini_bank
 * ============================================================
 */

#include <stdio.h>
#include <string.h>   /* for strcmp, strcpy */
#include <stdlib.h>   /* for exit() */

/* ─── CONSTANTS ─────────────────────────────────────────── */
#define MAX_ACCOUNTS   50    /* maximum number of accounts  */
#define PIN_LENGTH     5     /* 4 digits + null terminator  */
#define NAME_LENGTH    50
#define DATABASE_FILE  "accounts.db"

/* ─── STRUCT: one bank account ──────────────────────────── */
typedef struct {
    int    account_number;
    char   owner_name[NAME_LENGTH];
    char   pin[PIN_LENGTH];
    float  balance;
    int    is_active;        /* 1 = open, 0 = closed        */
} Account;

/* ─── GLOBAL DATA ───────────────────────────────────────── */
Account accounts[MAX_ACCOUNTS];
int     total_accounts = 0;

/* ─── FUNCTION PROTOTYPES ───────────────────────────────── */
void  print_banner(void);
void  print_menu(void);
int   find_account(int acc_no);
int   authenticate(int index);
void  create_account(void);
void  deposit(void);
void  withdraw(void);
void  check_balance(void);
void  transfer(void);
void  view_all_accounts(void);   /* admin / demo helper     */
int   load_database(void);
int   save_database(void);

/* =========================================================
 *  MAIN
 * ========================================================= */
int main(void) {
    print_banner();

    if (!load_database()) {
        /* Create demo data only on the first run. */
        accounts[0].account_number = 1001;
        strcpy(accounts[0].owner_name, "Alice Johnson");
        strcpy(accounts[0].pin, "1234");
        accounts[0].balance   = 5000.00f;
        accounts[0].is_active = 1;

        accounts[1].account_number = 1002;
        strcpy(accounts[1].owner_name, "Bob Smith");
        strcpy(accounts[1].pin, "5678");
        accounts[1].balance   = 2500.50f;
        accounts[1].is_active = 1;
        total_accounts = 2;

        if (!save_database()) {
            printf("[!] Could not create the account database.\n");
        }
    }

    int choice;

    while (1) {                          /* infinite loop — exit via option 7 */
        print_menu();
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1: create_account();  break;
            case 2: deposit();         break;
            case 3: withdraw();        break;
            case 4: check_balance();   break;
            case 5: transfer();        break;
            case 6: view_all_accounts(); break;
            case 7:
                save_database();
                printf("\nThank you for banking with Mini Bank. Goodbye!\n\n");
                exit(0);
            default:
                printf("\n[!] Invalid option. Please choose 1-7.\n");
        }
    }

    return 0;
}

/* =========================================================
 *  PRINT BANNER — decorative header
 * ========================================================= */
void print_banner(void) {
    printf("\n");
    printf("  ╔═════════════════════════════════=════╗\n");      
    printf("  ║        MINI BANK ATM SYSTEM          ║\n");
    printf("  ║      C Language Course Project       ║\n");
    printf("  ╚══════════════════════════════════════╝\n\n");
}

/* =========================================================
 *  PRINT MENU
 * ========================================================= */
void print_menu(void) {
    printf("\n  ┌─────────────────────────────┐\n");
    printf("  │         MAIN MENU           │\n");
    printf("  ├─────────────────────────────┤\n");
    printf("  │  1. Open New Account        │\n");
    printf("  │  2. Deposit Money           │\n");
    printf("  │  3. Withdraw Money          │\n");
    printf("  │  4. Check Balance           │\n");
    printf("  │  5. Transfer Funds          │\n");
    printf("  │  6. View All Accounts       │\n");
    printf("  │  7. Exit                    │\n");
    printf("  └─────────────────────────────┘\n");
}

/* =========================================================
 *  FIND ACCOUNT — returns index or -1 if not found
 * ========================================================= */
int find_account(int acc_no) {
    for (int i = 0; i < total_accounts; i++) {
        if (accounts[i].account_number == acc_no && accounts[i].is_active) {
            return i;   /* found! */
        }
    }
    return -1;          /* not found */
}

/* =========================================================
 *  DATABASE — simple persistent account storage
 *  Format: account_number|owner_name|pin|balance|is_active
 * ========================================================= */
int load_database(void) {
    FILE *file = fopen(DATABASE_FILE, "r");
    char line[NAME_LENGTH + 64];

    if (file == NULL) {
        return 0; /* First run: caller creates demo accounts. */
    }

    total_accounts = 0;
    while (total_accounts < MAX_ACCOUNTS && fgets(line, sizeof(line), file)) {
        Account *a = &accounts[total_accounts];

        if (sscanf(line, "%d|%49[^|]|%4[^|]|%f|%d",
                   &a->account_number,
                   a->owner_name,
                   a->pin,
                   &a->balance,
                   &a->is_active) == 5) {
            total_accounts++;
        }
    }

    fclose(file);
    return total_accounts > 0;
}

int save_database(void) {
    FILE *file = fopen(DATABASE_FILE, "w");

    if (file == NULL) {
        return 0;
    }

    for (int i = 0; i < total_accounts; i++) {
        if (fprintf(file, "%d|%s|%s|%.2f|%d\n",
                    accounts[i].account_number,
                    accounts[i].owner_name,
                    accounts[i].pin,
                    accounts[i].balance,
                    accounts[i].is_active) < 0) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

/* =========================================================
 *  AUTHENTICATE — asks for PIN, returns 1 (ok) or 0 (fail)
 * ========================================================= */
int authenticate(int index) {
    char entered_pin[PIN_LENGTH];
    printf("  Enter PIN: ");
    scanf("%4s", entered_pin);           /* read up to 4 chars */

    if (strcmp(accounts[index].pin, entered_pin) == 0) {
        printf("  [✓] PIN accepted.\n");
        return 1;
    } else {
        printf("  [✗] Incorrect PIN. Access denied.\n");
        return 0;
    }
}

/* =========================================================
 *  1. CREATE ACCOUNT
 * ========================================================= */
void create_account(void) {
    printf("\n  ── Open New Account ──\n");

    if (total_accounts >= MAX_ACCOUNTS) {
        printf("  [!] Bank is full. Cannot open more accounts.\n");
        return;
    }

    Account *a = &accounts[total_accounts];  /* pointer to next slot */
    int highest_account_number = 1000;

    for (int i = 0; i < total_accounts; i++) {
        if (accounts[i].account_number > highest_account_number) {
            highest_account_number = accounts[i].account_number;
        }
    }

    /* Assign account number automatically */
    a->account_number = highest_account_number + 1;
    a->is_active      = 1;

    /* Get owner name */
    printf("  Full Name : ");
    scanf(" %[^\n]", a->owner_name);        /* reads name with spaces */

    /* Get PIN */
    printf("  Choose 4-digit PIN: ");
    scanf("%4s", a->pin);

    /* Initial deposit */
    printf("  Initial Deposit (Rs): ");
    scanf("%f", &a->balance);

    if (a->balance < 0) {
        printf("  [!] Deposit cannot be negative. Account not created.\n");
        return;
    }

    total_accounts++;
    save_database();

    printf("\n  [✓] Account created successfully!\n");
    printf("  ┌────────────────────────────────┐\n");
    printf("  │ Account No : %d               │\n", a->account_number);
    printf("  │ Name       : %-16s  │\n",           a->owner_name);
    printf("  │ Balance    : Rs%-14.2f  │\n",        a->balance);
    printf("  └────────────────────────────────┘\n");
}

/* =========================================================
 *  2. DEPOSIT
 * ========================================================= */
void deposit(void) {
    printf("\n  ── Deposit Money ──\n");

    int acc_no;
    printf("  Account Number: ");
    scanf("%d", &acc_no);

    int idx = find_account(acc_no);
    if (idx == -1) {
        printf("  [!] Account not found.\n");
        return;
    }

    if (!authenticate(idx)) return;

    float amount;
    printf("  Deposit Amount (Rs): ");
    scanf("%f", &amount);

    if (amount <= 0) {
        printf("  [!] Amount must be positive.\n");
        return;
    }

    accounts[idx].balance += amount;
    save_database();
    printf("  [✓] Rs%.2f deposited. New balance: Rs%.2f\n",
           amount, accounts[idx].balance);
}

/* =========================================================
 *  3. WITHDRAW
 * ========================================================= */
void withdraw(void) {
    printf("\n  ── Withdraw Money ──\n");

    int acc_no;
    printf("  Account Number: ");
    scanf("%d", &acc_no);

    int idx = find_account(acc_no);
    if (idx == -1) {
        printf("  [!] Account not found.\n");
        return;
    }

    if (!authenticate(idx)) return;

    float amount;
    printf("  Withdrawal Amount (Rs): ");
    scanf("%f", &amount);

    if (amount <= 0) {
        printf("  [!] Amount must be positive.\n");
        return;
    }

    if (amount > accounts[idx].balance) {
        printf("  [!] Insufficient funds. Balance: Rs%.2f\n",
               accounts[idx].balance);
        return;
    }

    accounts[idx].balance -= amount;
    save_database();
    printf("  [✓] Rs%.2f withdrawn. Remaining balance: Rs%.2f\n",
           amount, accounts[idx].balance);
}

/* =========================================================
 *  4. CHECK BALANCE
 * ========================================================= */
void check_balance(void) {
    printf("\n  ── Check Balance ──\n");

    int acc_no;
    printf("  Account Number: ");
    scanf("%d", &acc_no);

    int idx = find_account(acc_no);
    if (idx == -1) {
        printf("  [!] Account not found.\n");
        return;
    }

    if (!authenticate(idx)) return;

    printf("\n  ┌──────────────────────────────────┐\n");
    printf("  │ Account : %d                     │\n", accounts[idx].account_number);
    printf("  │ Name    : %-20s  │\n",                 accounts[idx].owner_name);
    printf("  │ Balance : Rs%-18.2f  │\n",              accounts[idx].balance);
    printf("  └──────────────────────────────────┘\n");
}

/* =========================================================
 *  5. TRANSFER FUNDS
 * ========================================================= */
void transfer(void) {
    printf("\n  ── Transfer Funds ──\n");

    int from_no, to_no;
    printf("  Your Account Number  : ");
    scanf("%d", &from_no);

    int from = find_account(from_no);
    if (from == -1) {
        printf("  [!] Source account not found.\n");
        return;
    }

    if (!authenticate(from)) return;

    printf("  Recipient Account No : ");
    scanf("%d", &to_no);

    int to = find_account(to_no);
    if (to == -1) {
        printf("  [!] Recipient account not found.\n");
        return;
    }

    if (from == to) {
        printf("  [!] Cannot transfer to the same account.\n");
        return;
    }

    float amount;
    printf("  Transfer Amount (Rs)  : ");
    scanf("%f", &amount);

    if (amount <= 0) {
        printf("  [!] Amount must be positive.\n");
        return;
    }

    if (amount > accounts[from].balance) {
        printf("  [!] Insufficient funds. Balance: Rs%.2f\n",
               accounts[from].balance);
        return;
    }

    accounts[from].balance -= amount;
    accounts[to].balance   += amount;
    save_database();

    printf("  [✓] Rs%.2f transferred to account %d.\n", amount, to_no);
    printf("  Your new balance: Rs%.2f\n", accounts[from].balance);
}

/* =========================================================
 *  6. VIEW ALL ACCOUNTS (admin / demo)
 * ========================================================= */
void view_all_accounts(void) {
    printf("\n  ── All Accounts (Demo View) ──\n");

    if (total_accounts == 0) {
        printf("  No accounts exist yet.\n");
        return;
    }

    printf("  %-8s  %-20s  %-10s\n", "Acc No", "Name", "Balance");
    printf("  %-8s  %-20s  %-10s\n", "------", "----", "-------");

    for (int i = 0; i < total_accounts; i++) {
        if (accounts[i].is_active) {
            printf("  %-8d  %-20s  Rs%-9.2f\n",
                   accounts[i].account_number,
                   accounts[i].owner_name,
                   accounts[i].balance);
        }
    }
}
