#ifndef MAIL_H
#define MAIL_H

#include "anope.h"

extern CoreExport bool Mail(User *u, NickCore *nc, BotInfo *service, const Anope::string &subject, const Anope::string &message);
extern CoreExport bool Mail(NickCore *nc, const Anope::string &subject, const Anope::string &message);
extern CoreExport bool MailValidate(const Anope::string &email);

class MailThread : public Thread
{
 private:
	Anope::string MailTo;
	Anope::string Addr;
	Anope::string Subject;
	Anope::string Message;
	bool DontQuoteAddresses;

	bool Success;
 public:
	MailThread(const Anope::string &mailto, const Anope::string &addr, const Anope::string &subject, const Anope::string &message);

	~MailThread();

	void Run();
};

#endif // MAIL_H
