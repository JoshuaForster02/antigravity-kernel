#!/bin/bash
# =============================================================================
# Antigravity GitHub Setup
# Creates all three repos on GitHub and pushes initial commits.
#
# Usage:
#   chmod +x setup-github.sh
#   ./setup-github.sh
#
# Requires: git, curl
# You'll need a GitHub Personal Access Token with 'repo' scope:
#   https://github.com/settings/tokens/new
# =============================================================================

GITHUB_USER="JoshuaForster02"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PARENT_DIR="$( dirname "$SCRIPT_DIR" )"

echo ""
echo "+============================================================+"
echo "|         A N T I G R A V I T Y   G i t H u b   S e t u p  |"
echo "+============================================================+"
echo ""
echo "Enter your GitHub Personal Access Token (input hidden):"
echo "  -> Create one at: https://github.com/settings/tokens/new"
echo "  -> Scopes needed: repo"
read -s GITHUB_TOKEN
echo ""

if [ -z "$GITHUB_TOKEN" ]; then
    echo "[ERROR] No token provided. Exiting."
    exit 1
fi

# ── Helper: create repo via GitHub API ──────────────────────────────────────
create_github_repo() {
    local name=$1
    local description=$2
    echo "  >> Creating GitHub repo: $name ..."
    response=$(curl -s -X POST \
        -H "Authorization: token $GITHUB_TOKEN" \
        -H "Accept: application/vnd.github.v3+json" \
        https://api.github.com/user/repos \
        -d "{\"name\":\"$name\",\"description\":\"$description\",\"private\":false,\"auto_init\":false}")

    if echo "$response" | grep -q '"full_name"'; then
        echo "  >> OK: https://github.com/$GITHUB_USER/$name"
    else
        echo "  >> WARN: $(echo "$response" | grep -o '"message":"[^"]*"' | head -1)"
    fi
}

# ── Helper: init, commit and push a directory ────────────────────────────────
push_repo() {
    local dir=$1
    local remote=$2
    local commit_msg=$3
    cd "$dir"
    if [ ! -d ".git" ]; then
        git init
    fi
    git add -A
    git diff --cached --quiet || git commit -m "$commit_msg"
    git branch -M main
    git remote remove origin 2>/dev/null
    git remote add origin "$remote"
    git push -u origin main
    echo ""
}

# ── 1. antigravity-kernel ────────────────────────────────────────────────────
echo "[1/3] antigravity-kernel  (TRON bare-metal OS)"
create_github_repo "antigravity-kernel" "TRON-inspired bare-metal x86 OS kernel — Flynn's OS"
push_repo "$SCRIPT_DIR" \
    "https://github.com/$GITHUB_USER/antigravity-kernel.git" \
    "feat: Antigravity OS v0.1 — TRON-style bare-metal kernel"

# ── 2. antigravity-linux ─────────────────────────────────────────────────────
echo "[2/3] antigravity-linux   (custom Linux mini-distro)"
create_github_repo "antigravity-linux" "Antigravity Linux — custom minimal Linux-based distro"
mkdir -p "$PARENT_DIR/antigravity-linux"
cat > "$PARENT_DIR/antigravity-linux/README.md" << 'EOF'
# antigravity-linux

Custom minimal Linux-based operating system — Option B of the Antigravity project.

## Vision
- Linux kernel as base
- Custom init system and userspace
- Minimal footprint, custom shell
- Part of the [Antigravity](https://github.com/JoshuaForster02) project

## Status
🚧 In planning
EOF
push_repo "$PARENT_DIR/antigravity-linux" \
    "https://github.com/$GITHUB_USER/antigravity-linux.git" \
    "feat: Initial commit — antigravity-linux skeleton"

# ── 3. antigravity-app ───────────────────────────────────────────────────────
echo "[3/3] antigravity-app     (native macOS workspace app)"
create_github_repo "antigravity-app" "Antigravity macOS App — Notion, Anki & productivity workspace"
mkdir -p "$PARENT_DIR/antigravity-app"
cat > "$PARENT_DIR/antigravity-app/README.md" << 'EOF'
# antigravity-app

Native macOS application integrating Notion, Anki, and other productivity tools — Option C of the Antigravity project.

## Vision
- SwiftUI native macOS app
- Notion API integration
- Anki sync
- Unified workspace / launcher
- Part of the [Antigravity](https://github.com/JoshuaForster02) project

## Status
🚧 In planning
EOF
push_repo "$PARENT_DIR/antigravity-app" \
    "https://github.com/$GITHUB_USER/antigravity-app.git" \
    "feat: Initial commit — antigravity-app skeleton"

# ── Done ─────────────────────────────────────────────────────────────────────
echo "+============================================================+"
echo "|  All repos created and pushed!                            |"
echo "|  https://github.com/$GITHUB_USER                 |"
echo "+============================================================+"
echo ""
