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

class CommandOSOLine : public Command
{
 public:
	CommandOSOLine(Module *creator) : Command(creator, "operserv/oline", 2, 2)
	{
		this->SetDesc(_("Give Operflags to a certain user"));
		this->SetSyntax(_("\037nick\037 \037flags\037"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;
		const Anope::string &nick = params[0];
		const Anope::string &flag = params[1];
		User *u2 = NULL;

		/* let's check whether the user is online */
		if (!(u2 = finduser(nick)))
			source.Reply(NICK_X_NOT_IN_USE, nick.c_str());
		else if (u2 && flag[0] == '+')
		{
			ircdproto->SendSVSO(Config->OperServ, nick, flag);
			u2->SetMode(source.owner, UMODE_OPER);
			u2->SendMessage(source.owner, _("You are now an IRC Operator."));
			source.Reply(_("Operflags \002%s\002 have been added for \002%s\002."), flag.c_str(), nick.c_str());
			ircdproto->SendGlobops(source.owner, "\2%s\2 used OLINE for %s", u->nick.c_str(), nick.c_str());
		}
		else if (u2 && flag[0] == '-')
		{
			ircdproto->SendSVSO(Config->OperServ, nick, flag);
			source.Reply(_("Operflags \002%s\002 have been added for \002%s\002."), flag.c_str(), nick.c_str());
			ircdproto->SendGlobops(source.owner, "\2%s\2 used OLINE for %s", u->nick.c_str(), nick.c_str());
		}
		else
			this->OnSyntaxError(source, "");

		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Allows Services Opers to give Operflags to any user.\n"
				"Flags have to be prefixed with a \"+\" or a \"-\". To\n"
				"remove all flags simply type a \"-\" instead of any flags."));
		return true;
	}
};

class OSOLine : public Module
{
	CommandOSOLine commandosoline;

 public:
	OSOLine(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		commandosoline(this)
	{
		this->SetAuthor("Anope");

		if (!ircd || !ircd->omode)
			throw ModuleException("Your IRCd does not support OMODE.");

	}
};

MODULE_INIT(OSOLine)
