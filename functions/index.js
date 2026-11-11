"use strict";

/* adminResetPassword - lets the game admin reset ANOTHER player's
 * account password from inside the game (chat command:
 *   passwd <nick> <new password>
 * ).
 *
 * A game client can never do this by itself: Firebase Auth changes a
 * password only with that account's own ID token. This callable Cloud
 * Function is the server side - it verifies the caller's ID token,
 * checks the caller is one of the admin accounts, and only then uses
 * the Admin SDK to set the password.
 *
 * The password stored in Firebase Auth is "<player password>" +
 * CB4_PASS_SUFFIX (the same secret tail the game client appends, see
 * src/engine/network/write_secret.inc). The suffix lives ONLY in the
 * function secret, never in this file:
 *   firebase functions:secrets:set CB4_PASS_SUFFIX
 * (print the current value with: python3 tools/rotate_db_secret.py --show)
 *
 * Deploy:
 *   cd functions && npm install
 *   firebase deploy --only functions:adminResetPassword
 */

const { onCall, HttpsError } = require("firebase-functions/v2/https");
const { defineSecret } = require("firebase-functions/params");
const admin = require("firebase-admin");

admin.initializeApp();

const passSuffix = defineSecret("CB4_PASS_SUFFIX");

const ADMIN_EMAILS = ["dimasi4ek229@cb4.game"];
const EMAIL_DOMAIN = "@cb4.game";
const NICK_RE = /^[A-Za-z0-9_]{1,16}$/;

function bad(code, message) {
  return new HttpsError(code, message);
}

exports.adminResetPassword = onCall(
  { region: "us-central1", secrets: [passSuffix] },
  async (req) => {
    if (!req.auth || !req.auth.token || !req.auth.token.email) {
      throw bad("unauthenticated", "sign-in required");
    }
    const caller = String(req.auth.token.email).toLowerCase();
    if (!ADMIN_EMAILS.includes(caller)) {
      throw bad("permission-denied", "admins only");
    }

    const data = req.data || {};
    const nick = typeof data.nick === "string" ? data.nick.trim() : "";
    const password = typeof data.password === "string" ? data.password : "";
    if (!NICK_RE.test(nick)) {
      throw bad("invalid-argument", "bad nick");
    }
    if (
      password.length < 1 ||
      password.length > 32 ||
      /[\s"\\]/.test(password) ||
      /[\u0000-\u001F\u007F]/.test(password)
    ) {
      throw bad("invalid-argument", "bad password");
    }

    const email = nick.toLowerCase() + EMAIL_DOMAIN;
    if (ADMIN_EMAILS.includes(email)) {
      /* Admin accounts are protected, same as the in-game ban rule. */
      throw bad("permission-denied", "protected account");
    }

    let user;
    try {
      user = await admin.auth().getUserByEmail(email);
    } catch (e) {
      if (e && e.code === "auth/user-not-found") {
        throw bad("not-found", "no such account");
      }
      throw bad("internal", "lookup failed");
    }

    await admin.auth().updateUser(user.uid, {
      password: password + passSuffix.value(),
    });
    /* Kick the player's old sessions: the old password stops working
     * everywhere, not just on paper. */
    await admin.auth().revokeRefreshTokens(user.uid);

    return { ok: true };
  }
);
