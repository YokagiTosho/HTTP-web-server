#include "io.h"

int send_fd(int s, channel_t *channel, int size)
{
	int n;
	struct iovec iov[1];
	struct msghdr msg;
	union {
		struct cmsghdr cm;
		char           space[CMSG_SPACE(sizeof(int))];
	} cmsg;
	
	iov[0].iov_base = (char *) channel;
	iov[0].iov_len  = size;

	msg.msg_iov     = iov;
	msg.msg_iovlen  = 1;

	msg.msg_name    = NULL;
	msg.msg_namelen = 0;

#define CONTROLLEN CMSG_LEN(sizeof(int))

	if (channel->socket == -1) {
		msg.msg_control    = NULL;
		msg.msg_controllen = 0;
	} else {
		cmsg.cm.cmsg_level = SOL_SOCKET;
		cmsg.cm.cmsg_type = SCM_RIGHTS;
		cmsg.cm.cmsg_len  = CONTROLLEN;

		msg.msg_control = &cmsg;
		msg.msg_controllen = sizeof(cmsg);
		*(int *) CMSG_DATA(&cmsg.cm) = channel->socket;
	}

	n = sendmsg(s, &msg, 0);
	if (n == -1) {
		sus_log_error(LEVEL_PANIC, "Failed \"sendmsg()\": %s", strerror(errno));
		return SUS_ERROR;
	} else if (n == 0) {
		sus_log_error(LEVEL_PANIC, "Got 0 in \"sendmsg()\"");
		return SUS_ERROR;
	}
	return SUS_OK;
}

int recv_fd(int s, channel_t *channel, int size)
{
	int n;
	struct iovec iov[1];
	struct msghdr msg;
	union {
		struct cmsghdr cm;
		char           space[CMSG_SPACE(sizeof(int))];
	} cmsg;

	iov[0].iov_base = (char *) channel;
	iov[0].iov_len  = size;

	msg.msg_iov    = iov;
	msg.msg_iovlen = 1;

	msg.msg_name    = NULL;
	msg.msg_namelen = 0;

	msg.msg_control    = &cmsg;
	msg.msg_controllen = CONTROLLEN;

	n = recvmsg(s, &msg, 0);
	if (n == -1) {
		/* TODO */
		sus_log_error(LEVEL_PANIC, "Failed \"recvmsg()\" returned -1: %s", strerror(errno));
		return SUS_ERROR;
	}
	else if (n == 0) {
		/* TODO */
		sus_log_error(LEVEL_PANIC, "Failed \"recvmsg()\" returned 0: %s", strerror(errno));
		return SUS_ERROR;
	}

	if (msg.msg_controllen != CONTROLLEN) {
		sus_log_error(LEVEL_PANIC, "msg.msg_controllen != CONTROLLEN");
		return SUS_ERROR;
	}

	if (channel->cmd == WORKER_RECV_FD) {
		if (cmsg.cm.cmsg_len < CMSG_LEN(sizeof(int))) {
			sus_log_error(LEVEL_PANIC, "Ancillary data wrong size");
			return SUS_ERROR;
		}
		channel->socket = *(int *) CMSG_DATA(&cmsg.cm);
	}
	return n;
}
