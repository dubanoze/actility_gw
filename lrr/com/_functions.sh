#!/bin/sh
# vim:ft=sh:set noet ts=4 sw=4:

# Return codes
RET_FAILURE=1
RET_SUCCESS=0


#	_checkUseSftp
#
#	This function is used to check the availability of sftp and sshpass binaries
#
#	It takes no arguments
#
#	Returns: RET_SUCCESS or RET_FAILURE
#
_checkUseSftp () {
	EXESFTP=sftp
	EXESFTP_ACT=${ROOTACT}/lrr/sftp/sftp
	SSH_OPTIONS=" -oStrictHostKeyChecking=no -oBatchMode=no"
	type $EXESFTP > /dev/null 2>&1
	ret1=$?
	type $EXESFTP_ACT > /dev/null 2>&1
	ret2=$?
	if [ $ret1 -eq 0 ] || [ $ret2 -eq  0 ]; then
		if [ $ret1 != 0 ] ; then
			# If sftp not found in PATH, use the sftp binary provided with the LRR package
			EXESFTP=$EXESFTP_ACT
			# Specific case for wirmaar which use OpenSSH ssh binary with LRR sftp (unpatched) binary
			if	[ "$LRRSYSTEM" != "wirmaar" -a "$LRRSYSTEM" != "wirmana" ]; then
				SSH_OPTIONS=" -y -y "
			fi
		fi
	else
		echo "sftp binary not found !"
		return $RET_FAILURE
	fi

	EXESSHPASS=sshpass
	EXESSHPASS_ACT=${ROOTACT}/lrr/com/sshpass.x
	type $EXESSHPASS > /dev/null 2>&1
	ret1=$?
	type $EXESSHPASS_ACT > /dev/null 2>&1
	ret2=$?
	if [ $ret1 -eq 0 ] || [ $ret2 -eq  0 ]; then
		if [ $ret1 != 0 ] ; then
			EXESSHPASS=$EXESSHPASS_ACT
		fi
	else
		echo "sshpass binary not found !"
		return $RET_FAILURE
	fi
	return $RET_SUCCESS
}


#	_getArgs
#
#	This function is used to parse and extract the arguments given to download or upload functions.
#
#	It takes no arguments
#
#	Returns: RET_SUCCESS or RET_FAILURE
#
_getArgs() {

	t_local_file=""
	t_remote_file=""
	t_host=""
	t_port=""
	t_user=""
	t_password=""
	t_use_sftp=""

	while [ $# -gt 0 ]; do
		case $1 in
			-u)		shift
					t_user="${1}"
					;;
			-w) 	shift
					t_password="${1}"
					;;
			-a) 	shift
					t_host="${1}"
					;;
			-p)		shift
					t_port="${1}"
					;;
			-l)		shift
					t_local_file="${1}"
					;;
			-r)		shift
					t_remote_file="${1}"
					;;
			-s)		shift
					t_use_sftp="${1}"
					;;
			-c)		shift
					t_crypted="1"
					;;
			*)		shift ;;
		esac
	done

	if [ -z "${t_remote_file}" ]; then
		t_remote_file="${t_local_file}"
	fi

	if [ -z "${t_user}" ]; then
		echo "ERROR: User must be set (even for an anonymous mode)"
		return $RET_FAILURE
	fi


	if [ ! -z "${t_use_sftp}" ] && [ ${t_use_sftp} -eq 1 ]; then
		if [ -z "${t_password}" ]; then
			echo "ERROR: Password must be set for SFTP mode"
			return $RET_FAILURE
		fi
	fi

	# Specific use by suplog, password is encrypted, key is in environment var SUPLOGKEY
	if [ "$t_crypted" = "1" ]
	then
		DECRYPT="$ROOTACT/lrr/com/keycrypt.x -k SUPLOGKEY "
		t_password="$($DECRYPT $t_password)"
	fi

	return $RET_SUCCESS

}

#	lrr_DownloadFromRemote -u username -w password -l local_filename [-r remote_filename] -a host_address -p host_port [-s use_sftp]
#
#	This function is used to download a file from a remote server using either FTP or SFTP protocol.
#
#	Arguments:
#		-u username			remote host user login. Must be always set, even in case of anonymous FTP
#		-w password			remote host user password. Can be omitted in case of anonymous FTP. Mandatory in case of SFTP
#		-l local_filename	local file name
#		-r remote_filename	remote file name. If omitted, use the same path as local filename
#		-a host_address		remote host address
#		-p host_port		remote host port
#		-s use_sftp			set to 1 to use sftp. If 0 or not set, use classic ftp.
#
#	Returns: RET_FAILURE or the value returned by the transfert command
#
lrr_DownloadFromRemote() {
	if	[ $# -lt 6 ] ; then
		echo "lrr_DownloadFromRemote wrapper not enough arg"
		return $RET_FAILURE
	fi

	_getArgs $*
	if [ $? -eq $RET_FAILURE ]; then
		return $RET_FAILURE
	fi

	# If requested, try with sftp
	if [ ! -z "${t_use_sftp}" ] && [ ${t_use_sftp} -eq 1 ]; then
		_checkUseSftp
		if [ $? -eq $RET_SUCCESS ]; then
			echo "lrr_DownloadFromRemote uses ${EXESFTP} -P ${t_port} ${SSH_OPTIONS} \"${t_user}@${t_host}:${t_remote_file}\" ${t_local_file}"
			${EXESSHPASS} -p "${t_password}" ${EXESFTP} -P ${t_port} ${SSH_OPTIONS} "${t_user}@${t_host}:${t_remote_file}" "${t_local_file}"
			return $?
		else
			echo "Trying with classic ftp"
		fi
	fi

	# Try with ftp
	type curl > /dev/null 2>&1
	if [ $? = 0 ]; then
		echo "lrr_DownloadFromRemote uses curl"
		curl -o "${t_local_file}" ftp://${t_user}:"${t_password}"@${t_host}:${t_port}/"${t_remote_file}"
		return $?
	fi
	type wget > /dev/null 2>&1
	if [ $? = 0 ]; then
		echo "lrr_DownloadFromRemote uses wget"
		wget -O "${t_local_file}" ftp://${t_user}:"${t_password}"@${t_host}:${t_port}/"${t_remote_file}"
		return $?
	fi

	echo "no command found for ftp download"
	return $RET_FAILURE
}


#	lrr_UploadToRemote -u username -w password -l local_filename [-r remote_filename] -a host_address -p host_port [-s use_sftp]
#
#	This function is used to upload a file to a remote server using either FTP or SFTP protocol.
#
#	Arguments:
#		-u username			remote host user login. Must be always set, even in case of anonymous FTP
#		-w password			remote host user password. Can be omitted in case of anonymous FTP. Mandatory in case of SFTP
#		-l local_filename	local file name
#		-r remote_filename	remote file name. If omitted, use the same path as local filename
#		-a host_address		remote host address
#		-p host_port		remote host port
#		-s use_sftp			set to 1 to use sftp. If 0 or not set, use classic ftp.
#
#	Returns: RET_FAILURE or the value returned by the transfert command
#
lrr_UploadToRemote() {
	if	[ $# -lt 6 ] ; then
		echo "lrr_UploadToRemote wrapper not enough arg"
		return $RET_FAILURE
	fi

	_getArgs $*
	if [ $? -eq $RET_FAILURE ]; then
		return $RET_FAILURE
	fi

	# If asked, try with sftp
	if [ ! -z "${t_use_sftp}" ] && [ ${t_use_sftp} -eq 1 ]; then
		_checkUseSftp
		if [ $? -eq $RET_SUCCESS ]; then
			local tmp_sftp_batch_file=/tmp/sftp-batch-file
			echo "lrr_UploadToRemote uses ${EXESFTP} -P ${t_port} ${SSH_OPTIONS} -b ${tmp_sftp_batch_file} \"${t_user}@${t_host}\" "
			echo "put ${t_local_file} ${t_remote_file}" > ${tmp_sftp_batch_file}
			${EXESSHPASS} -p "${t_password}" ${EXESFTP} -P ${t_port} ${SSH_OPTIONS} -b ${tmp_sftp_batch_file} "${t_user}@${t_host}"
			ret=$?
			if [ $ret == 0 ]; then echo "End of transfer: Success using SFTP"; else echo "End of transfer: ERROR"; fi
			rm -f ${tmp_sftp_batch_file}
			return $ret
		else
			echo "Trying with classic ftp"
		fi
	fi

	# Try with ftp
	type curl > /dev/null 2>&1
	if [ $? = 0 ]; then
		echo "lrr_UploadToRemote uses curl"
		curl -T "${t_local_file}" ftp://${t_user}:"${t_password}"@${t_host}:${t_port}/"${t_remote_file}"
		if [ $? == 0 ]; then echo "End of transfer: Success using curl"; else echo "End of transfer: ERROR"; fi
		return $?
	fi
	type ftpput > /dev/null 2>&1
	if [ $? = 0 ]; then
		echo "lrr_UploadToRemote uses ftpput"
		ftpput -u ${t_user} -p "${t_password}" -P ${t_port} ${t_host} "${t_remote_file}" "${t_local_file}"
		if [ $? == 0 ]; then echo "End of transfer: Success using ftpput"; else echo "End of transfer: ERROR"; fi
		return $?
	fi

	echo "no command found for ftp upload"
	return $RET_FAILURE
}


lrr_CloseFiles()
{
	if [ "$SYSTEM" = "oielec" -o "$SYSTEM" = "natrbpi_usb_v1.0" -o "$SYSTEM" = "rbpi_v1.0" ]; then
		return
	fi
	for fd in $(ls /proc/$$/fd); do
		case "$fd" in
			0|1|2)
#			echo "do not close $fd"
			;;
			*)
#			echo "closing $fd"
			eval "exec $fd>&-"
			;;
		esac
	done
}


