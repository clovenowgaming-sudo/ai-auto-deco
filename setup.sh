#!/usr/bin/env bash
# Usage: bash setup.sh [repo-name]
set -e
REPO="${1:-ai-auto-deco}"

git config user.name  >/dev/null 2>&1 || git config user.name "Vinz"
git config user.email >/dev/null 2>&1 || git config user.email "vinz@users.noreply.github.com"

[ -d .git ] || git init -b main
git add -A
git commit -m "AI Auto Deco" || true
git branch -M main

gh repo create "$REPO" --public --source=. --remote=origin --push

echo "Waiting for GitHub Actions..."
sleep 10
RUN_ID=$(gh run list --limit 1 --json databaseId -q '.[0].databaseId')
gh run watch "$RUN_ID" --exit-status || { echo "Build failed. Logs: gh run view $RUN_ID --log-failed"; exit 1; }

mkdir -p out
gh run download "$RUN_ID" -D out
echo "Done. Mod file:"
find out -name '*.geode'
