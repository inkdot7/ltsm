/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Copyright (c) 2019-2020, GSI Helmholtz Centre for Heavy Ion Research
 */

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "fsqapi.h"
#include "log.h"
#include "common.h"

#define FSQ_ERROR(rc, str)					\
do {								\
	fsq_session->fsq_packet.fsq_error.rc = rc;		\
	strncpy(fsq_session->fsq_packet.fsq_error.strerror,	\
		str,						\
		FSQ_MAX_ERRMSG_LENGTH);				\
} while (0);							\


int fsq_send(struct fsq_session_t *fsq_session,
	     enum fsq_protocol_state_t fsq_protocol_state)
{
	int rc = 0;
	ssize_t bytes_send;

	if (!fsq_session || fsq_session->fd < 0)
		return -EINVAL;

	fsq_session->fsq_packet.state = fsq_protocol_state;
	bytes_send = write_size(fsq_session->fd, &fsq_session->fsq_packet,
				sizeof(struct fsq_packet_t));
	CT_DEBUG("[fd=%d] fsq_send %zd, expected size: %zd, state: '%s'",
		 fsq_session->fd,
		 bytes_send, sizeof(struct fsq_packet_t),
		 FSQ_PROTOCOL_STR(fsq_session->fsq_packet.state));
	if (bytes_send < 0) {
		rc = -errno;
		CT_ERROR(rc, "bytes_send < 0");
		goto out;
	}
	if ((size_t)bytes_send != sizeof(struct fsq_packet_t)) {
		rc = -ENOMSG;
		CT_ERROR(rc, "bytes_send != sizeof(struct fsq_packet_t)");
	}
out:

	return rc;
}

int fsq_recv(struct fsq_session_t *fsq_session,
	     enum fsq_protocol_state_t fsq_protocol_state)
{
	int rc = 0;
	ssize_t bytes_recv;

	if (!fsq_session || fsq_session->fd < 0)
		return -EINVAL;

	bytes_recv = read_size(fsq_session->fd,
			       &fsq_session->fsq_packet,
			       sizeof(struct fsq_packet_t));
	CT_DEBUG("[fd=%d] fsq_recv %zd, expected size: %zd, "
		 "state: '%s', expected: '%s'",
		 fsq_session->fd, bytes_recv,
		 sizeof(struct fsq_packet_t),
		 FSQ_PROTOCOL_STR(fsq_session->fsq_packet.state),
		 FSQ_PROTOCOL_STR(fsq_protocol_state));
	if (bytes_recv < 0) {
		rc = -errno;
		CT_ERROR(rc, "bytes_recv < 0");
		return rc;
	}
	if (bytes_recv != sizeof(struct fsq_packet_t)) {
		rc = -ENOMSG;
		CT_ERROR(rc, "bytes_recv != sizeof(struct fsq_packet_t)");
		return rc;
	}

	/* Verify received fsq packet matches the requested. */
	if (!(fsq_session->fsq_packet.state & fsq_protocol_state)) {
		rc = -EPROTO;
		CT_ERROR(rc, "fsq protocol error");
	}

	return rc;
}

int fsq_fconnect(struct fsq_login_t *fsq_login, struct fsq_session_t *fsq_session)
{
	int rc;
        struct sockaddr_in sockaddr_cli;
	struct hostent *hostent;

	hostent = gethostbyname(fsq_login->hostname);
	if (!hostent) {
		rc = -h_errno;
		CT_ERROR(rc, "%s", hstrerror(h_errno));
		goto out;
	}

        /* Connect to file storage queue (fsq). */
        fsq_session->fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fsq_session->fd < 0) {
                rc = -errno;
                CT_ERROR(rc, "socket");
                goto out;
        }

        memset(&sockaddr_cli, 0, sizeof(sockaddr_cli));
        sockaddr_cli.sin_family = AF_INET;
        sockaddr_cli.sin_addr = *((struct in_addr *)hostent->h_addr);
        sockaddr_cli.sin_port = htons(fsq_login->port);

	CT_INFO("connecting to '%s:%d'", fsq_login->hostname, fsq_login->port);
        rc = connect(fsq_session->fd,
		     (struct sockaddr *)&sockaddr_cli,
                     sizeof(sockaddr_cli));
        if (rc < 0) {
                rc = errno;
                CT_ERROR(rc, "connect");
                goto out;
        }

	memcpy(&fsq_session->fsq_packet.fsq_login,
	       fsq_login, sizeof(struct fsq_login_t));
	rc = fsq_send(fsq_session, FSQ_CONNECT);

out:
        if (rc)
                close(fsq_session->fd);

        return rc;
}

void fsq_fdisconnect(struct fsq_session_t *fsq_session)
{
	fsq_send(fsq_session, FSQ_DISCONNECT);
	close(fsq_session->fd);
}

static int __fsq_fopen(const char *fs, const char *fpath, const char *desc,
		       enum fsq_storage_dest_t fsq_storage_dest,
		       struct fsq_session_t *fsq_session)
{
	int rc = 0;
	struct fsq_info_t fsq_info = {
		.fs		   = {0},
		.fpath		   = {0},
		.desc		   = {0},
		.fsq_storage_dest  = fsq_storage_dest
	};

	if (!fsq_session)
		return -EFAULT;

	if (!(fs && fpath)) {
		close(fsq_session->fd);
		return -EFAULT;
	}

	/* Fillup struct fsq_info_t with fs, fpath, desc. */
	strncpy(fsq_info.fs, fs, DSM_MAX_FSNAME_LENGTH);
	strncpy(fsq_info.fpath, fpath, PATH_MAX);
	if (desc)
		strncpy(fsq_info.desc, desc, DSM_MAX_DESCR_LENGTH);
	memcpy(&(fsq_session->fsq_packet.fsq_info),
	       &fsq_info, sizeof(fsq_info));

	rc = fsq_send(fsq_session, FSQ_OPEN);
	if (rc)
		close(fsq_session->fd);

	return rc;
}

int fsq_fopen(const char *fs, const char *fpath, const char *desc,
	      struct fsq_session_t *fsq_session)
{
	return __fsq_fopen(fs, fpath, desc, FSQ_STORAGE_LUSTRE_TSM, fsq_session);
}

int fsq_fdopen(const char *fs, const char *fpath, const char *desc,
	       enum fsq_storage_dest_t fsq_storage_dest,
	       struct fsq_session_t *fsq_session)
{
	return __fsq_fopen(fs, fpath, desc, fsq_storage_dest, fsq_session);
}

ssize_t fsq_fwrite(const void *ptr, size_t size, size_t nmemb,
		   struct fsq_session_t *fsq_session)
{
	int rc;
	ssize_t bytes_written;

	fsq_session->fsq_packet.fsq_data.size = size * nmemb;

	rc = fsq_send(fsq_session, FSQ_DATA);
	if (rc) {
		close(fsq_session->fd);
		return rc;
	}

	bytes_written = write_size(fsq_session->fd, ptr,
				   fsq_session->fsq_packet.fsq_data.size);
	CT_DEBUG("[fd=%d] write size %zd, expected size %zd",
		 fsq_session->fd, bytes_written,
		 fsq_session->fsq_packet.fsq_data.size);

	return bytes_written;
}

int fsq_fclose(struct fsq_session_t *fsq_session)
{
	int rc = 0;

	rc = fsq_send(fsq_session, FSQ_CLOSE);
	if (rc)
		close(fsq_session->fd);

	memset(&fsq_session->fsq_packet, 0, sizeof(struct fsq_packet_t));

	return rc;
}
