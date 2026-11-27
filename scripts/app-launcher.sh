#!/bin/sh
# Install or remove a desktop launcher that runs `loadout ui`.
# macOS: an app bundle (default /Applications/Loadout.app).
# Linux: a desktop entry (default ~/.local/share/applications/loadout.desktop).
#
# Usage: app-launcher.sh install <loadout-binary> <launcher-path>
#        app-launcher.sh uninstall <launcher-path>
set -eu

install_macos_bundle() {
    binary=$1
    bundle=$2
    rm -rf "$bundle"
    mkdir -p "$bundle/Contents/MacOS"

    cat > "$bundle/Contents/MacOS/Loadout" <<EOF
#!/bin/sh
exec "$binary" ui
EOF
    chmod 755 "$bundle/Contents/MacOS/Loadout"

    cat > "$bundle/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>Loadout</string>
    <key>CFBundleDisplayName</key>
    <string>Loadout</string>
    <key>CFBundleIdentifier</key>
    <string>dev.loadout.app</string>
    <key>CFBundleExecutable</key>
    <string>Loadout</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF
}

install_linux_desktop_entry() {
    binary=$1
    entry=$2
    mkdir -p "$(dirname "$entry")"
    cat > "$entry" <<EOF
[Desktop Entry]
Type=Application
Name=Loadout
Comment=Pick the skills each coding agent loads
Exec="$binary" ui
Terminal=false
Categories=Development;
EOF
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$(dirname "$entry")" || true
    fi
}

command=${1:-}
case "$command" in
    install)
        binary=$2
        launcher=$3
        if [ ! -x "$binary" ]; then
            echo "app-launcher: $binary is not an executable. Run 'make release' first." >&2
            exit 1
        fi
        if [ "$(uname -s)" = "Darwin" ]; then
            install_macos_bundle "$binary" "$launcher"
        else
            install_linux_desktop_entry "$binary" "$launcher"
        fi
        echo "Installed $launcher -> $binary ui"
        ;;
    uninstall)
        launcher=$2
        rm -rf "$launcher"
        echo "Removed $launcher"
        ;;
    *)
        echo "Usage: $0 install <loadout-binary> <launcher-path> | uninstall <launcher-path>" >&2
        exit 1
        ;;
esac
