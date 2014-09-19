/* HostServ core functions
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

class CommandHSGroup : public Command
{
	void Sync(NickAlias *na)
	{
		if (!na || !na->hostinfo.HasVhost())
			return;
	
		for (std::list<NickAlias *>::iterator it = na->nc->aliases.begin(), it_end = na->nc->aliases.end(); it != it_end; ++it)
		{
			NickAlias *nick = *it;
			nick->hostinfo.SetVhost(na->hostinfo.GetIdent(), na->hostinfo.GetHost(), na->hostinfo.GetCreator());
		}
	}

 public:
	CommandHSGroup(Module *creator) : Command(creator, "hostserv/group", 0, 0)
	{
		this->SetDesc(_("Syncs the vhost for all nicks in a group"));
		this->SetSyntax("");
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;
		NickAlias *na = findnick(u->nick);
		if (na && u->Account() == na->nc && na->hostinfo.HasVhost())
		{
			this->Sync(na);
			if (!na->hostinfo.GetIdent().empty())
				source.Reply(_("All vhost's in the group \002%s\002 have been set to \002%s\002@\002%s\002"), u->Account()->display.c_str(), na->hostinfo.GetIdent().c_str(), na->hostinfo.GetHost().c_str());
			else
				source.Reply(_("All vhost's in the group \002%s\002 have been set to \002%s\002"), u->Account()->display.c_str(), na->hostinfo.GetHost().c_str());
		}
		else
			source.Reply(HOST_NOT_ASSIGNED);

		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("This command allows users to set the vhost of their\n"
				"CURRENT nick to be the vhost for all nicks in the same\n"
				"group."));
		return true;
	}
};

class HSGroup : public Module
{
	CommandHSGroup commandhsgroup;

 public:
	HSGroup(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		commandhsgroup(this)
	{
		this->SetAuthor("Anope");

	}
};

MODULE_INIT(HSGroup)
