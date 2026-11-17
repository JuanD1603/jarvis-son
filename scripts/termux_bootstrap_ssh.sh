#!/data/data/com.termux/files/usr/bin/bash
# Bootstrap de Termux: actualiza paquetes, instala openssh y termux-api, autoriza la clave del PC y arranca sshd.
set -e
# Uso: bash termux_bootstrap_ssh.sh "<clave publica ssh-ed25519 del PC>"
PUB="${1:?falta la clave publica}"
exec > >(tee $HOME/bootstrap_ssh.log) 2>&1
export DEBIAN_FRONTEND=noninteractive
yes | pkg upgrade -y -o Dpkg::Options::=--force-confnew
pkg install -y openssh termux-api
mkdir -p $HOME/.ssh && chmod 700 $HOME/.ssh
touch $HOME/.ssh/authorized_keys
grep -qF "$PUB" $HOME/.ssh/authorized_keys || echo "$PUB" >> $HOME/.ssh/authorized_keys
chmod 600 $HOME/.ssh/authorized_keys
grep -q 'jarvis-son autostart' $HOME/.bashrc 2>/dev/null || cat >> $HOME/.bashrc <<'RC'
# jarvis-son autostart
pgrep -x sshd >/dev/null || sshd
termux-wake-lock 2>/dev/null
RC
pgrep -x sshd >/dev/null || sshd
termux-wake-lock || true
echo BOOTSTRAP_OK
