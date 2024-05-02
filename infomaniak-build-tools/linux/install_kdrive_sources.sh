#!/bin/bash

# Convenience script to install desktop-kDrive source code

set -e

exec 3>&1 > /tmp/kdrive_source_installer.log 2>&1

sudo apt-get install xclip git 1> /dev/null

exec >&3 2>&1 3>&-

function ssh_prompt(){
	read -p "Do you already have an ssh key ? (y/n) " haveSSH
	if [ "$haveSSH" == "n" ]; then
		echo "We will generate a ssh key"
		read -p "Enter username for ssh key: " username

		read -p "Enter keyname : " keyname

		ssh-keygen -t rsa -b 4096 -C "$username" -f ~/.ssh/"$keyname"

		clear
		echo "Your key has been pasted in your clipboard."
		echo "Paste it in your github account: "
		echo ""
		cat ~/.ssh/"${keyname}.pub"
		cat ~/.ssh/"${keyname}.pub" | xclip -sel clip

		read -p "Have you done it ? (y/n) " confirm
		if [ "$confirm" != "y" ]; then
			echo "Operation cancelled by the user."
			exit
		fi
	fi
}

function install_kdrive_sources() {
	echo "Cloning desktop kDrive project ..." >&3

	mkdir -p ${HOME}/Projects
	cd ${HOME}/Projects
	ssh-keyscan gitlab.infomaniak.ch >> ~/.ssh/known_hosts
	git clone --recurse-submodules https://github.com/Infomaniak/desktop-kDrive.git
	
	echo "done." >&3
}

ssh_prompt
install_kdrive_sources

echo "Sources installation completed." >&3
