 /* POSIX emulation layer for Windows.
 *
 * Copyright (C) 2008-2012 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#include "services.h"

int pipe(int fds[2])
{
	sockaddrs localhost;

	localhost.pton(AF_INET, "127.0.0.1");

	int cfd = socket(AF_INET, SOCK_STREAM, 0), lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (cfd == -1 || lfd == -1)
	{
		close(cfd);
		close(lfd);
		return -1;
	}

	if (bind(lfd, &localhost.sa, localhost.size()) == -1)
	{
		close(cfd);
		close(lfd);
		return -1;
	}

	if (listen(lfd, 1) == -1)
	{
		close(cfd);
		close(lfd);
		return -1;
	}

	sockaddrs lfd_addr;
	socklen_t sz = sizeof(lfd_addr);
	getsockname(lfd, &lfd_addr.sa, &sz);

	if (connect(cfd, &lfd_addr.sa, lfd_addr.size()))
	{
		close(cfd);
		close(lfd);
		return -1;
	}

	int afd = accept(lfd, NULL, NULL);
	if (afd == -1)
	{
		close(cfd);
		return -1;
	}

	fds[0] = cfd;
	fds[1] = afd;
	
	return 0;
}

