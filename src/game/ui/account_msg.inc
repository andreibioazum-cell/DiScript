/* Persistent status message buffer independent of the two-frame string pool. */
static char acct_msg_buf[160];
static const char *acct_msg = "";
static void acct_msg_set(const char *text) {
    double i = 0;
    if (!text) {
        text = "";
    }
    while (text[(int)i] != 0 && i < (double)(sizeof(acct_msg_buf) - 1)) {
        acct_msg_buf[(int)i] = text[(int)i];
        i = i + 1;
    }
    acct_msg_buf[(int)i] = 0;
    acct_msg = acct_msg_buf;
}
