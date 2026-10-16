# AI Auto Deco (Geode mod) - Mod By Vinz :3

## Cara pakai di Termux
```bash
pkg install git gh unzip
gh auth login                 # sekali aja
unzip ai-auto-deco.zip && cd ai-auto-deco
bash setup.sh                 # bikin repo pake gh, push, build, download .geode
```
Update kode lalu build ulang:
```bash
git add -A && git commit -m "update" && git push
gh run watch && gh run download -D out
```
Salin file `.geode` ke `Android/media/com.geode.launcher/game/geode/mods/`.

## Di game
1. Buka editor, pilih block / structure.
2. Tekan tombol **AI** (kanan atas, kiri tombol Copy).
3. Ketik style deco -> **Deco!** (muncul loading + "Thinking...").
4. Ikon gear = settings: URL, API key, model, lalu tekan **Set**.

URL boleh `https://api.openai.com/v1`, `https://api.groq.com/openai/v1`, `.../api/v3`, atau full `.../chat/completions`.
Isi "Extra deco object IDs" dengan ID deco favoritmu biar AI bisa pakai.
