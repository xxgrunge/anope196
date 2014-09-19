/* NickServ core functions
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

class CommandNSIdentify : public Command
{
 public:
	CommandNSIdentify(Module *creator) : Command(creator, "nickserv/identify", 1, 2)
	{
		this->SetFlag(CFLAG_ALLOW_UNREGISTERED);
		this->SetDesc(_("Identify yourself with your password"));
		this->SetSyntax(_("[\037account\037] \037password\037"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		const Anope::string &nick = params.size() == 2 ? params[0] : u->nick;
		Anope::string pass = params[params.size() - 1];

		NickAlias *na = findnick(nick);
		if (na && na->nc->HasFlag(NI_SUSPENDED))
			source.Reply(NICK_X_SUSPENDED, na->nick.c_str());
		else if (u->Account() && na && u->Account() == na->nc)
			source.Reply(_("You are already identified."));
		else
		{
			EventReturn MOD_RESULT;
			FOREACH_RESULT(I_OnCheckAuthentication, OnCheckAuthentication(this, &source, params, na ? na->nc->display : nick, pass));
			if (MOD_RESULT == EVENT_STOP)
				return;

			if (!na)
				source.Reply(NICK_X_NOT_REGISTERED, nick.c_str());
			else if (MOD_RESULT != EVENT_ALLOW)
			{
				Log(LOG_COMMAND, u, this) << "and failed to identify";
				source.Reply(PASSWORD_INCORRECT);
				bad_password(u);
			}
			else
			{
				if (u->IsIdentified())
					Log(LOG_COMMAND, u, this) << "to log out of account " << u->Account()->display;

				Log(LOG_COMMAND, u, this) << "and identified for account " << na->nc->display;
				source.Reply(_("Password accepted - you are now recognized."));
				u->Identify(na);
			}
		}
		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Tells %s that you are really the owner of this\n"
				"nick.  Many commands require you to authenticate yourself\n"
				"with this command before you use them.  The password\n"
				"should be the same one you sent with the \002REGISTER\002\n"
				"command."), source.owner->nick.c_str());
		return true;
	}
};

class NSIdentify : public Module
{
	CommandNSIdentify commandnsidentify;

 public:
	NSIdentify(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		commandnsidentify(this)
	{
		this->SetAuthor("Anope");

	}
};

MODULE_INIT(NSIdentify)
