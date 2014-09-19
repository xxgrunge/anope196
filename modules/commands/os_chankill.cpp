/* OperServ core functions
 *
 * (C) 2003-2012 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "module.h"

static service_reference<XLineManager> akills("XLineManager", "xlinemanager/sgline");

class CommandOSChanKill : public Command
{
 public:
	CommandOSChanKill(Module *creator) : Command(creator, "operserv/chankill", 2, 3)
	{
		this->SetDesc(_("AKILL all users on a specific channel"));
		this->SetSyntax(_("[+\037expiry\037] \037channel\037 \037reason\037"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		if (!akills)
			return;

		User *u = source.u;
		Anope::string expiry, channel;
		time_t expires;
		unsigned last_param = 1;
		Channel *c;

		channel = params[0];
		if (!channel.empty() && channel[0] == '+')
		{
			expiry = channel;
			channel = params[1];
			last_param = 2;
		}

		expires = !expiry.empty() ? dotime(expiry) : Config->ChankillExpiry;
		if (!expiry.empty() && isdigit(expiry[expiry.length() - 1]))
			expires *= 86400;
		if (expires && expires < 60)
		{
			source.Reply(BAD_EXPIRY_TIME);
			return;
		}
		else if (expires > 0)
			expires += Anope::CurTime;

		if (params.size() <= last_param)
		{
			this->OnSyntaxError(source, "");
			return;
		}

		Anope::string reason = params[last_param];
		if (params.size() > last_param + 1)
			reason += params[last_param + 1];
		if (!reason.empty())
		{
			Anope::string realreason;
			if (Config->AddAkiller)
				realreason = "[" + u->nick + "] " + reason;
			else
				realreason = reason;

			if ((c = findchan(channel)))
			{
				for (CUserList::iterator it = c->users.begin(), it_end = c->users.end(); it != it_end; )
				{
					UserContainer *uc = *it++;

					if (uc->user->server == Me || uc->user->HasMode(UMODE_OPER))
						continue;

					XLine *x = new XLine("*@" + uc->user->host, u->nick, expires, realreason, XLineManager::GenerateUID());
					akills->AddXLine(x);
					akills->Check(uc->user);
				}

				Log(LOG_ADMIN, u, this) << "on " << c->name << " (" << realreason << ")";
			}
			else
				source.Reply(CHAN_X_NOT_IN_USE, channel.c_str());
		}
		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Puts an AKILL for every nick on the specified channel. It\n"
				"uses the entire and complete real ident@host for every nick,\n"
				"then enforces the AKILL."));
		return true;
	}
};

class OSChanKill : public Module
{
	CommandOSChanKill commandoschankill;

 public:
	OSChanKill(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		commandoschankill(this)
	{
		this->SetAuthor("Anope");

	}
};

MODULE_INIT(OSChanKill)
