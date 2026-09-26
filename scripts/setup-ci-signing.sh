#!/usr/bin/env bash
#
# One-time setup: put the signing credentials into GitHub organisation secrets
# so CI can sign and notarise tagged releases the same way the local script
# does.
#
# Nothing is written to your shell history, and the exported certificate lives
# in a temporary directory that is deleted on exit.
#
# Usage:
#   scripts/setup-ci-signing.sh                    org-wide (needs admin:org)
#   scripts/setup-ci-signing.sh owner/repo         just that repo (needs repo admin)
#
# Org-wide is preferable — every plugin repo added later inherits the secrets —
# but it needs a token scope the default login does not include:
#
#   gh auth refresh -h github.com -s admin:org
#
set -euo pipefail

TARGET="${1:-Friendlitronics}"

# A value containing "/" is a repository; anything else is an organisation.
if [[ "$TARGET" == */* ]]; then
    SCOPE_ARGS=(--repo "$TARGET")
    WHERE="repository $TARGET"
else
    SCOPE_ARGS=(--org "$TARGET" --visibility all)
    WHERE="organisation $TARGET"
fi

command -v gh >/dev/null || { echo "needs the GitHub CLI: brew install gh" >&2; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "run: gh auth login" >&2; exit 1; }

# --- work out who you are from the keychain, rather than asking ---
IDENTITY=$(security find-identity -v -p codesigning \
           | grep "Developer ID Application" | head -1 \
           | sed -E 's/^[^"]*"([^"]+)".*$/\1/')

[ -n "$IDENTITY" ] || { echo "no Developer ID Application certificate found in your keychain" >&2; exit 1; }

TEAM=$(printf '%s' "$IDENTITY" | sed -E 's/.*\(([A-Z0-9]+)\)$/\1/')

echo "certificate : $IDENTITY"
echo "team id     : $TEAM"
echo "secrets go to: $WHERE"
echo

read -rp "Apple ID email for notarisation: " APPLE_ID_VALUE
[ -n "$APPLE_ID_VALUE" ] || { echo "an Apple ID is required" >&2; exit 1; }

# Read the app-specific password without echoing it to the terminal.
read -rsp "App-specific password (xxxx-xxxx-xxxx-xxxx): " APP_PW; echo
[ -n "$APP_PW" ] || { echo "an app-specific password is required" >&2; exit 1; }

TMP=$(mktemp -d)
chmod 700 "$TMP"
trap 'rm -rf "$TMP"' EXIT

echo
echo "==> exporting the certificate (macOS may ask permission to read the key)"
P12_PASS=$(openssl rand -base64 24)
security export -t identities -f pkcs12 -P "$P12_PASS" -o "$TMP/cert.p12"

echo "==> storing secrets on the $WHERE"
base64 -i "$TMP/cert.p12"      | gh secret set MACOS_CERT_P12      "${SCOPE_ARGS[@]}"
printf '%s' "$P12_PASS"        | gh secret set MACOS_CERT_PASSWORD "${SCOPE_ARGS[@]}"
printf '%s' "$IDENTITY"        | gh secret set SIGN_ID             "${SCOPE_ARGS[@]}"
printf '%s' "$TEAM"            | gh secret set TEAM_ID             "${SCOPE_ARGS[@]}"
printf '%s' "$APPLE_ID_VALUE"  | gh secret set APPLE_ID            "${SCOPE_ARGS[@]}"
printf '%s' "$APP_PW"          | gh secret set APP_PASSWORD        "${SCOPE_ARGS[@]}"

echo
gh secret list "${SCOPE_ARGS[@]}" 2>/dev/null || true
echo
echo "Done. Tagged releases will now be signed and notarised by CI."
echo "The exported certificate has been deleted; the .p12 password was random"
echo "and exists only as the MACOS_CERT_PASSWORD secret. To rotate, re-run this."
