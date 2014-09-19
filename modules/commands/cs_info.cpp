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

class CommandCSInfo : public Command
{
	void CheckOptStr(Anope::string &buf, ChannelInfoFlag opt, const char *str, ChannelInfo *ci, NickCore *nc)
	{
		if (ci->HasFlag(opt))
		{
			if (!buf.empty())
				buf += ", ";

			buf += translate(nc, str);
		}
	}

 public:
	CommandCSInfo(Module *creator) : Command(creator, "chanserv/info", 1, 2)
	{
		this->SetFlag(CFLAG_ALLOW_UNREGISTERED);
		this->SetDesc(_("Lists information about the named registered channel"));
		this->SetSyntax(_("\037channel\037"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		const Anope::string &chan = params[0];

		User *u = source.u;
		ChannelInfo *ci = cs_findchan(params[0]);
		if (ci == NULL)
		{
			source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
			return;
		}

		bool has_auspex = u->IsIdentified() && u->HasPriv("chanserv/auspex");
		bool show_all = false;

		/* Should we show all fields? Only for sadmins and identified users */
		if (has_auspex || ci->AccessFor(u).HasPriv("INFO"))
			show_all = true;

		InfoFormatter info(u);

		source.Reply(CHAN_INFO_HEADER, chan.c_str());
		if (ci->GetFounder())
			info["Founder"] = ci->GetFounder()->display;

		if (show_all && ci->successor)
			info["Successor"] = ci->successor->display;

		if (!ci->desc.empty())
			info["Description"] = ci->desc;

		info["Registered"] = do_strftime(ci->time_registered);
		info["Last used"] = do_strftime(ci->last_used);

		ModeLock *secret = ci->GetMLock(CMODE_SECRET);
		if (!ci->last_topic.empty() && (show_all || ((!secret || secret->set == false) && (!ci->c || !ci->c->HasMode(CMODE_SECRET)))))
		{
			info["Last topic"] = ci->last_topic;
			info["Topic set by"] = ci->last_topic_setter;
		}

		if (show_all)
		{
			info["Ban type"] = stringify(ci->bantype);

			Anope::string optbuf;
			CheckOptStr(optbuf, CI_KEEPTOPIC, _("Topic Retention"), ci, u->Account());
			CheckOptStr(optbuf, CI_PEACE, _("Peace"), ci, u->Account());
			CheckOptStr(optbuf, CI_PRIVATE, _("Private"), ci, u->Account());
			CheckOptStr(optbuf, CI_RESTRICTED, _("Restricted Access"), ci, u->Account());
			CheckOptStr(optbuf, CI_SECURE, _("Secure"), ci, u->Account());
			CheckOptStr(optbuf, CI_SECUREFOUNDER, _("Secure Founder"), ci, u->Account());
			CheckOptStr(optbuf, CI_SECUREOPS, _("Secure Ops"), ci, u->Account());
			if (ci->HasFlag(CI_SIGNKICK))
				CheckOptStr(optbuf, CI_SIGNKICK, _("Signed kicks"), ci, u->Account());
			else
				CheckOptStr(optbuf, CI_SIGNKICK_LEVEL, _("Signed kicks"), ci, u->Account());
			CheckOptStr(optbuf, CI_TOPICLOCK, _("Topic Lock"), ci, u->Account());
			CheckOptStr(optbuf, CI_PERSIST, _("Persistant"), ci, u->Account());
			CheckOptStr(optbuf, CI_NO_EXPIRE, _("No expire"), ci, u->Account());

			info["Options"] = optbuf.empty() ? _("None") : optbuf;
			info["Mode lock"] = ci->GetMLockAsString(true);

			if (!ci->HasFlag(CI_NO_EXPIRE))
				info["Expires on"] = do_strftime(ci->last_used + Config->CSExpire);
		}
		if (ci->HasFlag(CI_SUSPENDED))
		{
			Anope::string *by = ci->GetExt<ExtensibleString *>("suspend_by"), *reason = ci->GetExt<ExtensibleString *>("suspend_reason");
			if (by != NULL)
				info["Suspended"] = Anope::printf("[%s] %s", by->c_str(), (reason && !reason->empty() ? reason->c_str() : NO_REASON));
		}

		FOREACH_MOD(I_OnChanInfo, OnChanInfo(source, ci, info, show_all));

		std::vector<Anope::string> replies;
		info.Process(replies);

		for (unsigned i = 0; i < replies.size(); ++i)
			source.Reply(replies[i]);

		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Lists information about the named registered channel,\n"
				"including its founder, time of registration, last time\n"
				"used, description, and mode lock, if any. If \002ALL\002 is\n"
				"specified, the entry message and successor will also\n"
				"be displayed."));
		return true;
	}
};

class CSInfo : public Module
{
	CommandCSInfo commandcsinfo;

 public:
	CSInfo(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		commandcsinfo(this)
	{
		this->SetAuthor("Anope");

	}
};

MODULE_INIT(CSInfo)
