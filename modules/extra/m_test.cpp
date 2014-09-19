#include "module.h"

class test : public Module
{
 public:
	test(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		
	//E User *do_nick(const Anope::string &source, const Anope::string &nick, const Anope::string &username, const Anope::string &host, const Anope::string &server, const Anope::string &realname, time_t ts, const Anope::string &ip, const Anope::string &vhost, const Anope::string &uid, const Anope::string &modes);

		do_nick("", "Adam2", "Adam", "test.host", "irc.foonet.com", "Adam", Anope::CurTime, "2001:470:d19b::701", "test.host", "", "+ix");
	}

	~test()
	{
		delete finduser("Adam2");
	}
};

MODULE_INIT(test)
