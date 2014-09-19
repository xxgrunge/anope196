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

class SXLineDelCallback : public NumberList
{
	XLineManager *xlm;
	Command *command;
	CommandSource &source;
	unsigned Deleted;
 public:
	SXLineDelCallback(XLineManager *x, Command *c, CommandSource &_source, const Anope::string &numlist) : NumberList(numlist, true), xlm(x), command(c), source(_source), Deleted(0)
	{
	}

	~SXLineDelCallback()
	{
		if (!Deleted)
			source.Reply(_("No matching entries on the %s list."), this->command->name.c_str());
		else if (Deleted == 1)
			source.Reply(_("Deleted 1 entry from the %s list."), this->command->name.c_str());
		else
			source.Reply(_("Deleted %d entries from the %s list."), Deleted, this->command->name.c_str());
	}

	void HandleNumber(unsigned Number)
	{
		if (!Number)
			return;

		XLine *x = this->xlm->GetEntry(Number - 1);

		if (!x)
			return;

		++Deleted;
		DoDel(this->xlm, source, x);
	}

	static void DoDel(XLineManager *xlm, CommandSource &source, XLine *x)
	{
		xlm->DelXLine(x);
	}
};

class CommandOSSXLineBase : public Command
{
 private:
 	virtual XLineManager* xlm() = 0;

	virtual void OnAdd(CommandSource &source, const std::vector<Anope::string> &params) = 0;

	void OnDel(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		if (!this->xlm() || this->xlm()->GetList().empty())
		{
			source.Reply(_("%s list is empty."), source.command.c_str());
			return;
		}

		const Anope::string &mask = params.size() > 1 ? params[1] : "";

		if (mask.empty())
		{
			this->OnSyntaxError(source, "DEL");
			return;
		}

		if (isdigit(mask[0]) && mask.find_first_not_of("1234567890,-") == Anope::string::npos)
		{
			SXLineDelCallback list(this->xlm(), this, source, mask);
			list.Process();
		}
		else
		{
			XLine *x = this->xlm()->HasEntry(mask);

			if (!x)
			{
				source.Reply(_("\002%s\002 not found on the %s list."), mask.c_str(), source.command.c_str());
				return;
			}

			FOREACH_MOD(I_OnDelXLine, OnDelXLine(u, x, this->xlm()));

			SXLineDelCallback::DoDel(this->xlm(), source, x);
			source.Reply(_("\002%s\002 deleted from the %s list."), mask.c_str(), source.command.c_str());
		}

		if (readonly)
			source.Reply(READ_ONLY_MODE);

		return;
	}

	void ProcessList(CommandSource &source, const std::vector<Anope::string> &params, ListFormatter &list)
	{
		if (!this->xlm() || this->xlm()->GetList().empty())
		{
			source.Reply(_("%s list is empty."), source.command.c_str());
			return;
		}

		const Anope::string &mask = params.size() > 1 ? params[1] : "";

		if (!mask.empty() && isdigit(mask[0]) && mask.find_first_not_of("1234567890,-") == Anope::string::npos)
		{
			class SXLineListCallback : public NumberList
			{
				XLineManager *xlm;
				ListFormatter &list;
			 public:
				SXLineListCallback(XLineManager *x, ListFormatter &_list, const Anope::string &numlist) : NumberList(numlist, false), xlm(x), list(_list)
				{
				}

				void HandleNumber(unsigned Number)
				{
					if (!Number)
						return;

					XLine *x = this->xlm->GetEntry(Number - 1);

					if (!x)
						return;

					ListFormatter::ListEntry entry;
					entry["Number"] = stringify(Number);
					entry["Mask"] = x->Mask;
					entry["By"] = x->By;
					entry["Created"] = do_strftime(x->Created);
					entry["Expires"] = expire_left(NULL, x->Expires);
					entry["Reason"] = x->Reason;
					list.addEntry(entry);
				}
			}
			sl_list(this->xlm(), list, mask);
			sl_list.Process();
		}
		else
		{
			for (unsigned i = 0, end = this->xlm()->GetCount(); i < end; ++i)
			{
				XLine *x = this->xlm()->GetEntry(i);

				if (mask.empty() || mask.equals_ci(x->Mask) || mask == x->UID || Anope::Match(x->Mask, mask))
				{
					ListFormatter::ListEntry entry;
					entry["Number"] = stringify(i + 1);
					entry["Mask"] = x->Mask;
					entry["By"] = x->By;
					entry["Created"] = do_strftime(x->Created);
					entry["Expires"] = expire_left(source.u->Account(), x->Expires);
					entry["Reason"] = x->Reason;
					list.addEntry(entry);
				}
			}
		}

		if (list.isEmpty())
			source.Reply(_("No matching entries on the %s list."), source.command.c_str());
		else
		{
			source.Reply(_("Current %s list:"), source.command.c_str());

			std::vector<Anope::string> replies;
			list.Process(replies);

			for (unsigned i = 0; i < replies.size(); ++i)
				source.Reply(replies[i]);
		}
	}

	void OnList(CommandSource &source, const std::vector<Anope::string> &params)
	{
		ListFormatter list;
		list.addColumn("Number").addColumn("Mask").addColumn("Reason");

		this->ProcessList(source, params, list);
	}

	void OnView(CommandSource &source, const std::vector<Anope::string> &params)
	{
		ListFormatter list;
		list.addColumn("Number").addColumn("Mask").addColumn("By").addColumn("Created").addColumn("Expires").addColumn("Reason");
		this->ProcessList(source, params, list);
	}

	void OnClear(CommandSource &source)
	{
		User *u = source.u;
		FOREACH_MOD(I_OnDelXLine, OnDelXLine(u, NULL, this->xlm()));

		for (unsigned i = this->xlm()->GetCount(); i > 0; --i)
		{
			XLine *x = this->xlm()->GetEntry(i - 1);
			this->xlm()->DelXLine(x);
		}

		source.Reply(_("The %s list has been cleared."), source.command.c_str());

		return;
	}
 public:
	CommandOSSXLineBase(Module *creator, const Anope::string &cmd) : Command(creator, cmd, 1, 4)
	{
		this->SetDesc(Anope::printf(_("Manipulate the %s list"), cmd.c_str()));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		const Anope::string &cmd = params[0];

		if (cmd.equals_ci("ADD"))
			return this->OnAdd(source, params);
		else if (cmd.equals_ci("DEL"))
			return this->OnDel(source, params);
		else if (cmd.equals_ci("LIST"))
			return this->OnList(source, params);
		else if (cmd.equals_ci("VIEW"))
			return this->OnView(source, params);
		else if (cmd.equals_ci("CLEAR"))
			return this->OnClear(source);
		else
			this->OnSyntaxError(source, "");

		return;
	}

	virtual bool OnHelp(CommandSource &source, const Anope::string &subcommand) = 0;
};

class CommandOSSNLine : public CommandOSSXLineBase
{
	XLineManager *xlm()
	{
		return this->snlines;
	}

	void OnAdd(CommandSource &source, const std::vector<Anope::string> &params)
	{
		if (!this->xlm() ||! ircd->snline)
		{
			source.Reply(_("Your IRCd does not support SNLINE"));
			return;
		}

		User *u = source.u;
		unsigned last_param = 2;
		Anope::string param, expiry;
		time_t expires;

		param = params.size() > 1 ? params[1] : "";
		if (!param.empty() && param[0] == '+')
		{
			expiry = param;
			param = params.size() > 2 ? params[2] : "";
			last_param = 3;
		}

		expires = !expiry.empty() ? dotime(expiry) : Config->SNLineExpiry;
		/* If the expiry given does not contain a final letter, it's in days,
		 * said the doc. Ah well.
		 */
		if (!expiry.empty() && isdigit(expiry[expiry.length() - 1]))
			expires *= 86400;
		/* Do not allow less than a minute expiry time */
		if (expires && expires < 60)
		{
			source.Reply(BAD_EXPIRY_TIME);
			return;
		}
		else if (expires > 0)
			expires += Anope::CurTime;

		if (param.empty())
		{
			this->OnSyntaxError(source, "ADD");
			return;
		}

		Anope::string rest = param;
		if (params.size() > last_param)
			rest += " " + params[last_param];

		if (rest.find(':') == Anope::string::npos)
		{
			this->OnSyntaxError(source, "ADD");
			return;
		}

		sepstream sep(rest, ':');
		Anope::string mask;
		sep.GetToken(mask);
		Anope::string reason = sep.GetRemaining();

		if (!mask.empty() && !reason.empty())
		{
			std::pair<int, XLine *> canAdd = this->xlm()->CanAdd(mask, expires);
			if (mask.find_first_not_of("*?") == Anope::string::npos)
				source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
			else if (canAdd.first == 1)
				source.Reply(_("\002%s\002 already exists on the %s list."), canAdd.second->Mask.c_str(), source.command.c_str());
			else if (canAdd.first == 2)
				source.Reply(_("Expiry time of \002%s\002 changed."), canAdd.second->Mask.c_str());
			else if (canAdd.first == 3)
				source.Reply(_("\002%s\002 is already covered by %s."), mask.c_str(), canAdd.second->Mask.c_str());
			else
			{
				/* Clean up the last character of the mask if it is a space
				 * See bug #761
				 */
				unsigned masklen = mask.length();
				if (mask[masklen - 1] == ' ')
					mask.erase(masklen - 1);
				unsigned int affected = 0;
				for (Anope::insensitive_map<User *>::iterator it = UserListByNick.begin(); it != UserListByNick.end(); ++it)
					if (Anope::Match(it->second->realname, mask))
						++affected;
				float percent = static_cast<float>(affected) / static_cast<float>(UserListByNick.size()) * 100.0;

				if (percent > 95)
				{
					source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
					Log(LOG_ADMIN, u, this) << "tried to " << source.command << " " << percent << "% of the network (" << affected << " users)";
					return;
				}

				if (Config->AddAkiller)
					reason = "[" + u->nick + "] " + reason;

				Anope::string id;
				if (Config->AkillIds)
				{
					id = XLineManager::GenerateUID();
					reason = reason + " (ID: " + id + ")";
				}

				XLine *x = new XLine(mask, u->nick, expires, reason, id);

				EventReturn MOD_RESULT;
				FOREACH_RESULT(I_OnAddXLine, OnAddXLine(u, x, this->xlm()));
				if (MOD_RESULT == EVENT_STOP)
				{
					delete x;
					return;
				}

				this->xlm()->AddXLine(x);
				if (Config->KillonSNline && !ircd->sglineenforce)
				{
					Anope::string rreason = "G-Lined: " + reason;

					for (Anope::insensitive_map<User *>::const_iterator it = UserListByNick.begin(); it != UserListByNick.end();)
					{
						User *user = it->second;
						++it;

						if (!user->HasMode(UMODE_OPER) && user->server != Me && Anope::Match(user->realname, x->Mask))
							user->Kill(Config->ServerName, rreason);
					}
				}

				source.Reply(_("\002%s\002 added to the %s list."), mask.c_str(), source.command.c_str());
				Log(LOG_ADMIN, u, this) << "on " << mask << " (" << reason << ") expires in " << (expires ? duration(expires - Anope::CurTime) : "never") << " [affects " << affected << " user(s) (" << percent << "%)]";

				if (readonly)
					source.Reply(READ_ONLY_MODE);
			}

		}
		else
			this->OnSyntaxError(source, "ADD");

		return;
	}

	service_reference<XLineManager> snlines;
 public:
 	CommandOSSNLine(Module *creator) : CommandOSSXLineBase(creator, "operserv/snline"), snlines("XLineManager", "xlinemanager/snline")
	{
		this->SetSyntax(_("ADD [+\037expiry\037] \037mask\037:\037reason\037"));
		this->SetSyntax(_("DEL {\037mask\037 | \037entry-num\037 | \037list\037 | \037id\037}"));
		this->SetSyntax(_("LIST [\037mask\037 | \037list\037 | \037id\037]"));
		this->SetSyntax(_("VIEW [\037mask\037 | \037list\037 | \037id\037]"));
		this->SetSyntax(_("CLEAR"));
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Allows Services operators to manipulate the SNLINE list.  If\n"
				"a user with a realname matching an SNLINE mask attempts to \n"
				"connect, Services will not allow it to pursue his IRC\n"
				"session.\n"));
		source.Reply(_(" \n"
				"\002SNLINE ADD\002 adds the given realname mask to the SNLINE\n"
				"list for the given reason (which \002must\002 be given).\n"
				"\037expiry\037 is specified as an integer followed by one of \037d\037 \n"
				"(days), \037h\037 (hours), or \037m\037 (minutes).  Combinations (such as \n"
				"\0371h30m\037) are not permitted.  If a unit specifier is not \n"
				"included, the default is days (so \037+30\037 by itself means 30 \n"
				"days).  To add an SNLINE which does not expire, use \037+0\037.  If the\n"
				"realname mask to be added starts with a \037+\037, an expiry time must\n"
				"be given, even if it is the same as the default.  The\n"
				"current SNLINE default expiry time can be found with the\n"
				"\002STATS AKILL\002 command.\n"
				"Note: because the realname mask may contain spaces, the\n"
				"separator between it and the reason is a colon.\n"));
		source.Reply(_(" \n"
				"The \002SNLINE DEL\002 command removes the given mask from the\n"
				"SNLINE list if it is present.  If a list of entry numbers is \n"
				"given, those entries are deleted.  (See the example for LIST \n"
				"below.)\n"
				" \n"
				"The \002SNLINE LIST\002 command displays the SNLINE list.  \n"
				"If a wildcard mask is given, only those entries matching the\n"
				"mask are displayed.  If a list of entry numbers is given,\n"
				"only those entries are shown; for example:\n"
				"   \002SNLINE LIST 2-5,7-9\002\n"
				"      Lists SNLINE entries numbered 2 through 5 and 7 \n"
				"      through 9.\n"
				" \n"
				"\002SNLINE VIEW\002 is a more verbose version of \002SNLINE LIST\002, and \n"
				"will show who added an SNLINE, the date it was added, and when \n"
				"it expires, as well as the realname mask and reason.\n"
				" \n"
				"\002SNLINE CLEAR\002 clears all entries of the SNLINE list."));
		return true;
	}
};

class CommandOSSQLine : public CommandOSSXLineBase
{
	XLineManager *xlm()
	{
		return this->sqlines;
	}

	void OnAdd(CommandSource &source, const std::vector<Anope::string> &params)
	{
		if (!this->xlm() || !ircd->sqline)
		{
			source.Reply(_("Your IRCd does not support SQLINE"));
			return;
		}

		User *u = source.u;
		unsigned last_param = 2;
		Anope::string expiry, mask;
		time_t expires;

		mask = params.size() > 1 ? params[1] : "";
		if (!mask.empty() && mask[0] == '+')
		{
			expiry = mask;
			mask = params.size() > 2 ? params[2] : "";
			last_param = 3;
		}

		expires = !expiry.empty() ? dotime(expiry) : Config->SQLineExpiry;
		/* If the expiry given does not contain a final letter, it's in days,
		 * said the doc. Ah well.
		 */
		if (!expiry.empty() && isdigit(expiry[expiry.length() - 1]))
			expires *= 86400;
		/* Do not allow less than a minute expiry time */
		if (expires && expires < 60)
		{
			source.Reply(BAD_EXPIRY_TIME);
			return;
		}
		else if (expires > 0)
			expires += Anope::CurTime;

		if (params.size() <= last_param)
		{
			this->OnSyntaxError(source, "ADD");
			return;
		}

		Anope::string reason = params[last_param];
		if (last_param == 2 && params.size() > 3)
			reason += " " + params[3];
		if (!mask.empty() && !reason.empty())
		{
			std::pair<int, XLine *> canAdd = this->sqlines->CanAdd(mask, expires);
			if (mask.find_first_not_of("*") == Anope::string::npos)
				source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
			else if (canAdd.first == 1)
				source.Reply(_("\002%s\002 already exists on the SQLINE list."), canAdd.second->Mask.c_str());
			else if (canAdd.first == 2)
				source.Reply(_("Expiry time of \002%s\002 changed."), canAdd.second->Mask.c_str());
			else if (canAdd.first == 3)
				source.Reply(_("\002%s\002 is already covered by %s."), mask.c_str(), canAdd.second->Mask.c_str());
			else
			{
				unsigned int affected = 0;
				for (Anope::insensitive_map<User *>::iterator it = UserListByNick.begin(); it != UserListByNick.end(); ++it)
					if (Anope::Match(it->second->nick, mask))
						++affected;
				float percent = static_cast<float>(affected) / static_cast<float>(UserListByNick.size()) * 100.0;

				if (percent > 95)
				{
					source.Reply(USERHOST_MASK_TOO_WIDE, mask.c_str());
					Log(LOG_ADMIN, u, this) << "tried to SQLine " << percent << "% of the network (" << affected << " users)";
					return;
				}

				Anope::string id = XLineManager::GenerateUID();
				reason = reason + " (ID: " + id + ")";

				XLine *x = new XLine(mask, u->nick, expires, reason, id);

				EventReturn MOD_RESULT;
				FOREACH_RESULT(I_OnAddXLine, OnAddXLine(u, x, this->xlm()));
				if (MOD_RESULT == EVENT_STOP)
				{
					delete x;
					return;
				}

				this->xlm()->AddXLine(x);
				if (Config->KillonSQline)
				{
					Anope::string rreason = "Q-Lined: " + reason;

					if (mask[0] == '#')
					{
						for (channel_map::const_iterator cit = ChannelList.begin(), cit_end = ChannelList.end(); cit != cit_end; ++cit)
						{
							Channel *c = cit->second;

							if (!Anope::Match(c->name, mask))
								continue;
							for (CUserList::iterator it = c->users.begin(), it_end = c->users.end(); it != it_end; )
							{
								UserContainer *uc = *it;
								++it;

								if (uc->user->HasMode(UMODE_OPER) || uc->user->server == Me)
									continue;
								c->Kick(NULL, uc->user, "%s", reason.c_str());
							}
						}
					}
					else
					{
						for (Anope::insensitive_map<User *>::const_iterator it = UserListByNick.begin(); it != UserListByNick.end();)
						{
							User *user = it->second;
							++it;

							if (!user->HasMode(UMODE_OPER) && user->server != Me && Anope::Match(user->nick, x->Mask))
								user->Kill(Config->ServerName, rreason);
						}
					}
				}
				this->xlm()->Send(NULL, x);

				source.Reply(_("\002%s\002 added to the SQLINE list."), mask.c_str());
				Log(LOG_ADMIN, u, this) << "on " << mask << " (" << reason << ") expires in " << (expires ? duration(expires - Anope::CurTime) : "never") << " [affects " << affected << " user(s) (" << percent << "%)]";

				if (readonly)
					source.Reply(READ_ONLY_MODE);
			}

		}
		else
			this->OnSyntaxError(source, "ADD");

		return;
	}

 	service_reference<XLineManager> sqlines;
 public:
	CommandOSSQLine(Module *creator) : CommandOSSXLineBase(creator, "operserv/sqline"), sqlines("XLineManager", "xlinemanager/sqline")
	{
		this->SetSyntax(_("ADD [+\037expiry\037] \037mask\037 \037reason\037"));
		this->SetSyntax(_("DEL {\037mask\037 | \037entry-num\037 | \037list\037 | \037id\037}"));
		this->SetSyntax(_("LIST [\037mask\037 | \037list\037 | \037id\037]"));
		this->SetSyntax(_("VIEW [\037mask\037 | \037list\037 | \037id\037]"));
		this->SetSyntax(_("CLEAR"));
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Allows Services operators to manipulate the SQLINE list.  If\n"
				"a user with a nick matching an SQLINE mask attempts to \n"
				"connect, Services will not allow it to pursue his IRC\n"
				"session.\n"
				"If the first character of the mask is #, services will \n"
				"prevent the use of matching channels (on IRCds that \n"
				"support it).\n"));
		source.Reply(_(" \n"
				"\002SQLINE ADD\002 adds the given (nick's) mask to the SQLINE\n"
				"list for the given reason (which \002must\002 be given).\n"
				"\037expiry\037 is specified as an integer followed by one of \037d\037 \n"
				"(days), \037h\037 (hours), or \037m\037 (minutes). Combinations (such as \n"
				"\0371h30m\037) are not permitted. If a unit specifier is not \n"
				"included, the default is days (so \037+30\037 by itself means 30 \n"
				"days). To add an SQLINE which does not expire, use \037+0\037. \n"
				"If the mask to be added starts with a \037+\037, an expiry time \n"
				"must be given, even if it is the same as the default. The\n"
				"current SQLINE default expiry time can be found with the\n"
				"\002STATS AKILL\002 command.\n"));
		source.Reply(_(" \n"
				"The \002SQLINE DEL\002 command removes the given mask from the\n"
				"SQLINE list if it is present. If a list of entry numbers is \n"
				"given, those entries are deleted. (See the example for LIST \n"
				"below.)\n"
				" \n"
				"The \002SQLINE LIST\002 command displays the SQLINE list. \n"
				"If a wildcard mask is given, only those entries matching the\n"
				"mask are displayed. If a list of entry numbers is given,\n"
				"only those entries are shown; for example:\n"
				"   \002SQLINE LIST 2-5,7-9\002\n"
				"      Lists SQLINE entries numbered 2 through 5 and 7 \n"
				"      through 9.\n"
				" \n"
				"\002SQLINE VIEW\002 is a more verbose version of \002SQLINE LIST\002, and \n"
				"will show who added an SQLINE, the date it was added, and when \n"
				"it expires, as well as the mask and reason.\n"
				" \n"
				"\002SQLINE CLEAR\002 clears all entries of the SQLINE list."));
		return true;
	}
};

class OSSXLine : public Module
{
	CommandOSSNLine commandossnline;
	CommandOSSQLine commandossqline;

 public:
	OSSXLine(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		commandossnline(this), commandossqline(this)
	{
		this->SetAuthor("Anope");
	}
};

MODULE_INIT(OSSXLine)
