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
 * Copyright (c) 2019, GSI Helmholtz Centre for Heavy Ion Research
 */

#ifndef FSDAPI_H
#define FSDAPI_H

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

#define XATTR_FSD_PREFIX	"user.fsd"
#define XATTR_FSD_STATE		XATTR_FSD_PREFIX".state"
#define XATTR_FSD_ARCHIVE_ID	XATTR_FSD_PREFIX".arvid"
#define XATTR_FSD_DESC		XATTR_FSD_PREFIX".desc"

#define FSD_PROTOCOL_STR(s)							   \
	s == FSD_CONNECT    ? "FSD_CONNECT"    :				   \
	s == FSD_OPEN       ? "FSD_OPEN"       :				   \
	s == FSD_DATA       ? "FSD_DATA"       :				   \
	s == FSD_CLOSE      ? "FSD_CLOSE"      :				   \
	s == FSD_DISCONNECT ? "FSD_DISCONNECT" :				   \
	s == (FSD_DATA | FSD_CLOSE) ? "FSD_DATA | FSD_CLOSE" :                     \
	s == (FSD_DISCONNECT | FSD_OPEN) ? "FSD_DISCONNECT | FSD_OPEN" : "UNKNOWN" \

#define FSD_ACTION_STR(s)						     \
	s == STATE_FSD_COPY_DONE     ? "STATE_FSD_COPY_DONE"     :	     \
	s == STATE_LUSTRE_COPY_RUN   ? "STATE_LUSTRE_COPY_RUN"   :	     \
	s == STATE_LUSTRE_COPY_ERROR ? "STATE_LUSTRE_COPY_ERROR" :	     \
	s == STATE_LUSTRE_COPY_DONE  ? "STATE_LUSTRE_COPY_DONE"  :	     \
	s == STATE_TSM_ARCHIVE_RUN   ? "STATE_TSM_ARCHIVE_RUN"   :	     \
	s == STATE_TSM_ARCHIVE_ERROR ? "STATE_TSM_ARCHIVE_ERROR" :           \
	s == STATE_TSM_ARCHIVE_DONE  ? "STATE_TSM_ARCHIVE_DONE"  :           \
	s == STATE_FILE_OMITTED      ? "STATE_FILE_OMITTED"      : "UNKNOWN" \

enum fsd_action_state_t {
	STATE_FSD_COPY_DONE	= 0x1,
	STATE_LUSTRE_COPY_RUN	= 0x2,
	STATE_LUSTRE_COPY_ERROR = 0x4,
	STATE_LUSTRE_COPY_DONE  = 0x8,
	STATE_TSM_ARCHIVE_RUN	= 0x10,
	STATE_TSM_ARCHIVE_ERROR	= 0x20,
	STATE_TSM_ARCHIVE_DONE	= 0x40,
	STATE_FILE_OMITTED      = 0x80
};

enum fsd_protocol_state_t {
	FSD_CONNECT    = 0x1,
	FSD_OPEN       = 0x2,
	FSD_DATA       = 0x4,
	FSD_CLOSE      = 0x8,
	FSD_DISCONNECT = 0x10
};

struct fsd_login_t {
	char node[DSM_MAX_NODE_LENGTH + 1];
	char password[DSM_MAX_VERIFIER_LENGTH + 1];
	char hostname[HOST_NAME_MAX + 1];
	int port;
};

struct fsd_info_t {
	char fs[DSM_MAX_FSNAME_LENGTH + 1];
	char fpath[PATH_MAX + 1];
	char desc[DSM_MAX_DESCR_LENGTH + 1];
};

struct fsd_session_t {
	enum fsd_protocol_state_t state;
	struct fsd_login_t fsd_login;
	struct fsd_info_t fsd_info;
	int sock_fd;
	size_t size;
};

struct fsd_action_item_t {
	uint32_t fsd_action_state;
	struct fsd_info_t fsd_info;
	char fpath_local[PATH_MAX + 1];
	size_t size;
	size_t progress_size;
	time_t ts[3];
	size_t action_error_cnt;
	int archive_id;
};

int fsd_send(struct fsd_session_t *fsd_session,
	     const enum fsd_protocol_state_t protocol_state);
int fsd_recv(int fd, struct fsd_session_t *fsd_session,
	     enum fsd_protocol_state_t fsd_protocol_state);

void fsd_init(struct fsd_login_t *fsd_login, const char *servername,
	      const char *node, const char *password,
	      const char *owner, const char *platform,
	      const char *fsname, const char *fstype,
	      const char *hostname, const int port);
int fsd_fconnect(struct fsd_login_t *fsd_login,
		 struct fsd_session_t *fsd_session);
void fsd_fdisconnect(struct fsd_session_t *fsd_session);
int fsd_fopen(const char *fs, const char *fpath, const char *desc,
	      struct fsd_session_t *fsd_session);
ssize_t fsd_fwrite(const void *ptr, size_t size, size_t nmemb,
		   struct fsd_session_t *fsd_session);
int fsd_fclose(struct fsd_session_t *fsd_session);

#endif /* FSDAPI_H */
