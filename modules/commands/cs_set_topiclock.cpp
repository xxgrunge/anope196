/* ChanServ core functions
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

class CommandCSSetTopicLock : public Command
{
 public:
	CommandCSSetTopicLock(Module *creator, const Anope::string &cname = "chanserv/set/topiclock") : Command(creator, cname, 2, 2)
	{
		this->SetDesc(_("Topic can only be changed with TOPIC"));
		this->SetSyntax(_("\037channel\037 {ON | OFF}"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;
		ChannelInfo *ci = cs_findchan(params[0]);
		if (ci == NULL)
		{
			source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
			return;
		}

		if (source.permission.empty() && !ci->AccessFor(u).HasPriv("SET"))
		{
			source.Reply(ACCESS_DENIED);
			return;
		}

		if (params[1].equals_ci("ON"))
		{
			ci->SetFlag(CI_TOPICLOCK);
			source.Reply(_("Topic lock option for %s is now \002on\002."), ci->name.c_str());
		}
		else if (params[1].equals_ci("OFF"))
		{
			ci->UnsetFlag(CI_TOPICLOCK);
			source.Reply(_("Topic lock option for %s is now \002off\002."), ci->name.c_str());
		}
		else
			this->OnSyntaxError(source, "TOPICLOCK");

		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Enables or disables the \002topic lock\002 option for a channel.\n"
				"When \002topic lock\002 is set, the channel topic will be unchangable\n"
				" except via the \002TOPIC\002 command."));
		return true;
	}
};

class CommandCSSASetTopicLock : public CommandCSSetTopicLock
{
 public:
	CommandCSSASetTopicLock(Module *creator) : CommandCSSetTopicLock(creator, "chanserv/saset/topiclock")
	{
	}
};

class CSSetTopicLock : public Module
{
	CommandCSSetTopicLock commandcssettopiclock;
	CommandCSSASetTopicLock commandcssasettopiclock;

 public:
	CSSetTopicLock(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		commandcssettopiclock(this), commandcssasettopiclock(this)
	{
		this->SetAuthor("Anope");

	}
};

MODULE_INIT(CSSetTopicLock)
