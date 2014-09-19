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

static std::map<Anope::string, int16_t, ci::less> defaultLevels;

static void reset_levels(ChannelInfo *ci)
{
	ci->ClearLevels();
	for (std::map<Anope::string, int16_t, ci::less>::iterator it = defaultLevels.begin(), it_end = defaultLevels.end(); it != it_end; ++it)
		ci->SetLevel(it->first, it->second);
}

class AccessChanAccess : public ChanAccess
{
 public:
	int level;

 	AccessChanAccess(AccessProvider *p) : ChanAccess(p)
	{
	}

	bool HasPriv(const Anope::string &name) const
	{
		return this->ci->GetLevel(name) != ACCESS_INVALID && this->level >= this->ci->GetLevel(name);
	}

	Anope::string Serialize()
	{
		return stringify(this->level);
	}

	void Unserialize(const Anope::string &data)
	{
		this->level = convertTo<int>(data);
	}

	static int DetermineLevel(ChanAccess *access)
	{
		if (access->provider->name == "access/access")
		{
			AccessChanAccess *aaccess = debug_cast<AccessChanAccess *>(access);
			return aaccess->level;
		}
		else
		{
			int highest = 1;
			const std::vector<Privilege> &privs = PrivilegeManager::GetPrivileges();

			for (unsigned i = 0; i < privs.size(); ++i)
			{
				const Privilege &p = privs[i];
				if (access->ci->GetLevel(p.name) > highest && access->HasPriv(p.name))
					highest = access->ci->GetLevel(p.name);
			}

			if (highest >= ACCESS_FOUNDER)
				highest = ACCESS_FOUNDER - 1;

			return highest;
		}
	}
};

class AccessAccessProvider : public AccessProvider
{
 public:
	AccessAccessProvider(Module *o) : AccessProvider(o, "access/access")
	{
	}

	ChanAccess *Create()
	{
		return new AccessChanAccess(this);
	}
};

class CommandCSAccess : public Command
{
	void DoAdd(CommandSource &source, ChannelInfo *ci, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		Anope::string mask = params[2];
		int level = ACCESS_INVALID;

		try
		{
			level = convertTo<int>(params[3]);
		}
		catch (const ConvertException &) { }

		if (!level)
		{
			source.Reply(_("Access level must be non-zero."));
			return;
		}

		AccessGroup u_access = ci->AccessFor(u);
		ChanAccess *highest = u_access.Highest();
		int u_level = (highest ? AccessChanAccess::DetermineLevel(highest) : 0);
		if (level >= u_level && !u_access.Founder && !u->HasPriv("chanserv/access/modify"))
		{
			source.Reply(ACCESS_DENIED);
			return;
		}
		else if (level <= ACCESS_INVALID || level >= ACCESS_FOUNDER)
		{
			source.Reply(CHAN_ACCESS_LEVEL_RANGE, ACCESS_INVALID + 1, ACCESS_FOUNDER - 1);
			return;
		}

		bool override = !ci->AccessFor(u).HasPriv("ACCESS_CHANGE") || (level >= u_level && !u_access.Founder);

		if (mask.find_first_of("!*@") == Anope::string::npos && findnick(mask) == NULL)
			mask += "!*@*";

		for (unsigned i = ci->GetAccessCount(); i > 0; --i)
		{
			ChanAccess *access = ci->GetAccess(i - 1);
			if (mask.equals_ci(access->mask))
			{
				/* Don't allow lowering from a level >= u_level */
				if (AccessChanAccess::DetermineLevel(access) >= u_level && !u_access.Founder && !u->HasPriv("chanserv/access/modify"))
				{
					source.Reply(ACCESS_DENIED);
					return;
				}
				ci->EraseAccess(i - 1);
				break;
			}
		}

		if (ci->GetAccessCount() >= Config->CSAccessMax)
		{
			source.Reply(_("Sorry, you can only have %d access entries on a channel."), Config->CSAccessMax);
			return;
		}

		service_reference<AccessProvider> provider("AccessProvider", "access/access");
		if (!provider)
			return;
		AccessChanAccess *access = debug_cast<AccessChanAccess *>(provider->Create());
		access->ci = ci;
		access->mask = mask;
		access->creator = u->nick;
		access->level = level;
		access->last_seen = 0;
		access->created = Anope::CurTime;
		ci->AddAccess(access);

		FOREACH_MOD(I_OnAccessAdd, OnAccessAdd(ci, u, access));

		Log(override ? LOG_OVERRIDE : LOG_COMMAND, u, this, ci) << "to add " << mask << " with level " << level;
		source.Reply(_("\002%s\002 added to %s access list at level \002%d\002."), access->mask.c_str(), ci->name.c_str(), level);

		return;
	}

	void DoDel(CommandSource &source, ChannelInfo *ci, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		const Anope::string &mask = params[2];

		if (!ci->GetAccessCount())
			source.Reply(_("%s access list is empty."), ci->name.c_str());
		else if (isdigit(mask[0]) && mask.find_first_not_of("1234567890,-") == Anope::string::npos)
		{
			class AccessDelCallback : public NumberList
			{
				CommandSource &source;
				ChannelInfo *ci;
				Command *c;
				unsigned Deleted;
				Anope::string Nicks;
				bool Denied;
				bool override;
			 public:
				AccessDelCallback(CommandSource &_source, ChannelInfo *_ci, Command *_c, const Anope::string &numlist) : NumberList(numlist, true), source(_source), ci(_ci), c(_c), Deleted(0), Denied(false), override(false)
				{
					if (!ci->AccessFor(source.u).HasPriv("ACCESS_CHANGE") && source.u->HasPriv("chanserv/access/modify"))
						this->override = true;
				}

				~AccessDelCallback()
				{
					if (Denied && !Deleted)
						source.Reply(ACCESS_DENIED);
					else if (!Deleted)
						source.Reply(_("No matching entries on %s access list."), ci->name.c_str());
					else
					{
						Log(override ? LOG_OVERRIDE : LOG_COMMAND, source.u, c, ci) << "to delete " << Nicks;

						if (Deleted == 1)
							source.Reply(_("Deleted 1 entry from %s access list."), ci->name.c_str());
						else
							source.Reply(_("Deleted %d entries from %s access list."), Deleted, ci->name.c_str());
					}
				}

				void HandleNumber(unsigned Number)
				{
					if (!Number || Number > ci->GetAccessCount())
						return;

					User *user = source.u;

					ChanAccess *access = ci->GetAccess(Number - 1);

					AccessGroup u_access = ci->AccessFor(user);
					ChanAccess *u_highest = u_access.Highest();

					if ((u_highest ? AccessChanAccess::DetermineLevel(u_highest) : 0) <= AccessChanAccess::DetermineLevel(access) && !u_access.Founder && !this->override && !access->mask.equals_ci(user->Account()->display))
					{
						Denied = true;
						return;
					}

					++Deleted;
					if (!Nicks.empty())
						Nicks += ", " + access->mask;
					else
						Nicks = access->mask;

					FOREACH_MOD(I_OnAccessDel, OnAccessDel(ci, user, access));

					ci->EraseAccess(Number - 1);
				}
			}
			delcallback(source, ci, this, mask);
			delcallback.Process();
		}
		else
		{
			AccessGroup u_access = ci->AccessFor(u);
			ChanAccess *highest = u_access.Highest();
			int u_level = (highest ? AccessChanAccess::DetermineLevel(highest) : 0);

			for (unsigned i = ci->GetAccessCount(); i > 0; --i)
			{
				ChanAccess *access = ci->GetAccess(i - 1);
				if (mask.equals_ci(access->mask))
				{
					int access_level = AccessChanAccess::DetermineLevel(access);
					if (!access->mask.equals_ci(u->Account()->display) && !u_access.Founder && u_level <= access_level && !u->HasPriv("chanserv/access/modify"))
						source.Reply(ACCESS_DENIED);
					else
					{
						source.Reply(_("\002%s\002 deleted from %s access list."), access->mask.c_str(), ci->name.c_str());
						bool override = !u_access.Founder && !u_access.HasPriv("ACCESS_CHANGE") && !access->mask.equals_ci(u->Account()->display);
						Log(override ? LOG_OVERRIDE : LOG_COMMAND, u, this, ci) << "to delete " << access->mask;

						FOREACH_MOD(I_OnAccessDel, OnAccessDel(ci, u, access));
						ci->EraseAccess(access);
					}
					return;
				}
			}

			source.Reply(_("\002%s\002 not found on %s access list."), mask.c_str(), ci->name.c_str());
		}

		return;
	}

	void ProcessList(CommandSource &source, ChannelInfo *ci, const std::vector<Anope::string> &params, ListFormatter &list)
	{
		const Anope::string &nick = params.size() > 2 ? params[2] : "";

		if (!ci->GetAccessCount())
			source.Reply(_("%s access list is empty."), ci->name.c_str());
		else if (!nick.empty() && nick.find_first_not_of("1234567890,-") == Anope::string::npos)
		{
			class AccessListCallback : public NumberList
			{
				ListFormatter &list;
				ChannelInfo *ci;

			 public:
				AccessListCallback(ListFormatter &_list, ChannelInfo *_ci, const Anope::string &numlist) : NumberList(numlist, false), list(_list), ci(_ci)
				{
				}

				void HandleNumber(unsigned number)
				{
					if (!number || number > ci->GetAccessCount())
						return;

					ChanAccess *access = ci->GetAccess(number - 1);

					Anope::string timebuf;
					if (ci->c)
						for (CUserList::const_iterator cit = ci->c->users.begin(), cit_end = ci->c->users.end(); cit != cit_end; ++cit)
							if (access->Matches((*cit)->user, (*cit)->user->Account()))
								timebuf = "Now";
					if (timebuf.empty())
					{
						if (access->last_seen == 0)
							timebuf = "Never";
						else
							timebuf = do_strftime(access->last_seen, NULL, true);
					}

					ListFormatter::ListEntry entry;
					entry["Number"] = stringify(number);
					entry["Level"] = stringify(AccessChanAccess::DetermineLevel(access));
					entry["Mask"] = access->mask;
					entry["By"] = access->creator;
					entry["Last seen"] = timebuf;
					this->list.addEntry(entry);
				}
			}
			nl_list(list, ci, nick);
			nl_list.Process();
		}
		else
		{
			for (unsigned i = 0, end = ci->GetAccessCount(); i < end; ++i)
			{
				ChanAccess *access = ci->GetAccess(i);

				if (!nick.empty() && !Anope::Match(access->mask, nick))
					continue;

				Anope::string timebuf;
				if (ci->c)
					for (CUserList::const_iterator cit = ci->c->users.begin(), cit_end = ci->c->users.end(); cit != cit_end; ++cit)
						if (access->Matches((*cit)->user, (*cit)->user->Account()))
							timebuf = "Now";
				if (timebuf.empty())
				{
					if (access->last_seen == 0)
						timebuf = "Never";
					else
						timebuf = do_strftime(access->last_seen, NULL, true);
				}

				ListFormatter::ListEntry entry;
				entry["Number"] = stringify(i + 1);
				entry["Level"] = stringify(AccessChanAccess::DetermineLevel(access));
				entry["Mask"] = access->mask;
				entry["By"] = access->creator;
				entry["Last seen"] = timebuf;
				list.addEntry(entry);
			}
		}

		if (list.isEmpty())
			source.Reply(_("No matching entries on %s access list."), ci->name.c_str());
		else
		{
			std::vector<Anope::string> replies;
			list.Process(replies);

			source.Reply(_("Access list for %s:"), ci->name.c_str());
	
			for (unsigned i = 0; i < replies.size(); ++i)
				source.Reply(replies[i]);

			source.Reply(_("End of access list"));
		}

		return;
	}

	void DoList(CommandSource &source, ChannelInfo *ci, const std::vector<Anope::string> &params)
	{
		if (!ci->GetAccessCount())
		{
			source.Reply(_("%s access list is empty."), ci->name.c_str());
			return;
		}

		ListFormatter list;
		list.addColumn("Number").addColumn("Level").addColumn("Mask");
		this->ProcessList(source, ci, params, list);
	}

	void DoView(CommandSource &source, ChannelInfo *ci, const std::vector<Anope::string> &params)
	{
		if (!ci->GetAccessCount())
		{
			source.Reply(_("%s access list is empty."), ci->name.c_str());
			return;
		}

		ListFormatter list;
		list.addColumn("Number").addColumn("Level").addColumn("Mask").addColumn("By").addColumn("Last seen");
		this->ProcessList(source, ci, params, list);
	}

	void DoClear(CommandSource &source, ChannelInfo *ci)
	{
		User *u = source.u;

		if (!IsFounder(u, ci) && !u->HasPriv("chanserv/access/modify"))
			source.Reply(ACCESS_DENIED);
		else
		{
			FOREACH_MOD(I_OnAccessClear, OnAccessClear(ci, u));

			ci->ClearAccess();

			source.Reply(_("Channel %s access list has been cleared."), ci->name.c_str());

			bool override = !IsFounder(u, ci);
			Log(override ? LOG_OVERRIDE : LOG_COMMAND, u, this, ci) << "to clear the access list";
		}

		return;
	}

 public:
	CommandCSAccess(Module *creator) : Command(creator, "chanserv/access", 2, 4)
	{
		this->SetDesc(_("Modify the list of privileged users"));
		this->SetSyntax(_("\037channel\037 ADD \037mask\037 \037level\037"));
		this->SetSyntax(_("\037channel\037 DEL {\037mask\037 | \037entry-num\037 | \037list\037}"));
		this->SetSyntax(_("\037channel\037 LIST [\037mask\037 | \037list\037]"));
		this->SetSyntax(_("\037channel\037 VIEW [\037mask\037 | \037list\037]"));
		this->SetSyntax(_("\037channel\037 CLEAR\002"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		const Anope::string &cmd = params[1];
		const Anope::string &nick = params.size() > 2 ? params[2] : "";
		const Anope::string &s = params.size() > 3 ? params[3] : "";

		User *u = source.u;
		ChannelInfo *ci = cs_findchan(params[0]);
		if (ci == NULL)
		{
			source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
			return;
		}

		bool is_list = cmd.equals_ci("LIST") || cmd.equals_ci("VIEW");
		bool is_clear = cmd.equals_ci("CLEAR");
		bool is_del = cmd.equals_ci("DEL");

		bool has_access = false;
		if (u->HasPriv("chanserv/access/modify"))
			has_access = true;
		else if (is_list && ci->AccessFor(u).HasPriv("ACCESS_LIST"))
			has_access = true;
		else if (ci->AccessFor(u).HasPriv("ACCESS_CHANGE"))
			has_access = true;
		else if (is_del)
		{
			NickAlias *na = findnick(nick);
			if (na && na->nc == u->Account())
				has_access = true;
		}

		/* If LIST, we don't *require* any parameters, but we can take any.
		 * If DEL, we require a nick and no level.
		 * Else (ADD), we require a level (which implies a nick). */
		if (is_list || is_clear ? 0 : (cmd.equals_ci("DEL") ? (nick.empty() || !s.empty()) : s.empty()))
			this->OnSyntaxError(source, cmd);
		else if (!has_access)
			source.Reply(ACCESS_DENIED);
		else if (readonly && !is_list)
			source.Reply(_("Sorry, channel access list modification is temporarily disabled."));
		else if (cmd.equals_ci("ADD"))
			this->DoAdd(source, ci, params);
		else if (cmd.equals_ci("DEL"))
			this->DoDel(source, ci, params);
		else if (cmd.equals_ci("LIST"))
			this->DoList(source, ci, params);
		else if (cmd.equals_ci("VIEW"))
			this->DoView(source, ci, params);
		else if (cmd.equals_ci("CLEAR"))
			this->DoClear(source, ci);
		else
			this->OnSyntaxError(source, "");

		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Maintains the \002access list\002 for a channel.  The access\n"
				"list specifies which users are allowed chanop status or\n"
				"access to %s commands on the channel.  Different\n"
				"user levels allow for access to different subsets of\n"
				"privileges. Any registered user not on the access list has\n"
				"a user level of 0, and any unregistered user has a user level\n"
				"of -1."), source.owner->nick.c_str());
		source.Reply(" ");
		source.Reply(_("The \002ACCESS ADD\002 command adds the given mask to the\n"
				"access list with the given user level; if the mask is\n"
				"already present on the list, its access level is changed to\n"
				"the level specified in the command.  The \037level\037 specified\n"
				"must be less than that of the user giving the command, and\n"
				"if the \037mask\037 is already on the access list, the current\n"
				"access level of that nick must be less than the access level\n"
				"of the user giving the command. When a user joins the channel\n"
				"the access they receive is from the highest level entry in the\n"
				"access list."));
		source.Reply(" ");
		source.Reply(_("The \002ACCESS DEL\002 command removes the given nick from the\n"
				"access list.  If a list of entry numbers is given, those\n"
				"entries are deleted.  (See the example for LIST below.)\n"
				"You may remove yourself from an access list, even if you\n"
				"do not have access to modify that list otherwise."));
		source.Reply(" ");
		source.Reply(_("The \002ACCESS LIST\002 command displays the access list.  If\n"
				"a wildcard mask is given, only those entries matching the\n"
				"mask are displayed.  If a list of entry numbers is given,\n"
				"only those entries are shown; for example:\n"
				"   \002ACCESS #channel LIST 2-5,7-9\002\n"
				"      Lists access entries numbered 2 through 5 and\n"
				"      7 through 9.\n"
				" \n"
				"The \002ACCESS VIEW\002 command displays the access list similar\n"
				"to \002ACCESS LIST\002 but shows the creator and last used time.\n"
				" \n"
				"The \002ACCESS CLEAR\002 command clears all entries of the\n"
				"access list."));
		source.Reply(_("\002User access levels\002\n"
				" \n"
				"By default, the following access levels are defined:\n"
				" \n"
				"   \002Founder\002   Full access to %s functions; automatic\n"
				"                     opping upon entering channel.  Note\n"
				"                     that only one person may have founder\n"
				"                     status (it cannot be given using the\n"
				"                     \002ACCESS\002 command).\n"
				"   \002     10\002   Access to AKICK command; automatic opping.\n"
				"   \002      5\002   Automatic opping.\n"
				"   \002      3\002   Automatic voicing.\n"
				"   \002      0\002   No special privileges; can be opped by other\n"
				"                     ops (unless \002secure-ops\002 is set).\n"
				" \n"
				"These levels may be changed, or new ones added, using the\n"
				"\002LEVELS\002 command; type \002%s%s HELP LEVELS\002 for\n"
				"information."), source.owner->nick.c_str(), Config->UseStrictPrivMsgString.c_str(), source.owner->nick.c_str());
		return true;
	}
};

class CommandCSLevels : public Command
{
	void DoSet(CommandSource &source, ChannelInfo *ci, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		const Anope::string &what = params[2];
		const Anope::string &lev = params[3];

		int level;

		if (lev.equals_ci("FOUNDER"))
			level = ACCESS_FOUNDER;
		else
		{
			try
			{
				level = convertTo<int>(lev);
			}
			catch (const ConvertException &)
			{
				this->OnSyntaxError(source, "SET");
				return;
			}
		}

		if (level <= ACCESS_INVALID || level > ACCESS_FOUNDER)
			source.Reply(_("Level must be between %d and %d inclusive."), ACCESS_INVALID + 1, ACCESS_FOUNDER - 1);
		else
		{
			Privilege *p = PrivilegeManager::FindPrivilege(what);
			if (p == NULL)
				source.Reply(_("Setting \002%s\002 not known.  Type \002%s%s HELP LEVELS \002 for a list of valid settings."), what.c_str(), Config->UseStrictPrivMsgString.c_str(), source.owner->nick.c_str());
			else
			{
				ci->SetLevel(p->name, level);
				FOREACH_MOD(I_OnLevelChange, OnLevelChange(u, ci, p->name, level));

				bool override = !ci->AccessFor(u).HasPriv("FOUNDER");
				Log(override ? LOG_OVERRIDE : LOG_COMMAND, u, this, ci) << "to set " << p->name << " to level " << level;

				if (level == ACCESS_FOUNDER)
					source.Reply(_("Level for %s on channel %s changed to founder only."), p->name.c_str(), ci->name.c_str());
				else
					source.Reply(_("Level for \002%s\002 on channel %s changed to \002%d\002."), p->name.c_str(), ci->name.c_str(), level);
			}
		}
	}

	void DoDisable(CommandSource &source, ChannelInfo *ci, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		const Anope::string &what = params[2];

		/* Don't allow disabling of the founder level. It would be hard to change it back if you dont have access to use this command */
		if (!what.equals_ci("FOUNDER"))
		{
			Privilege *p = PrivilegeManager::FindPrivilege(what);
			if (p != NULL)
			{
				ci->SetLevel(p->name, ACCESS_INVALID);
				FOREACH_MOD(I_OnLevelChange, OnLevelChange(u, ci, p->name, ACCESS_INVALID));

				bool override = !ci->AccessFor(u).HasPriv("FOUNDER");
				Log(override ? LOG_OVERRIDE : LOG_COMMAND, u, this, ci) << "to disable " << p->name;

				source.Reply(_("\002%s\002 disabled on channel %s."), p->name.c_str(), ci->name.c_str());
				return;
			}
		}

		source.Reply(_("Setting \002%s\002 not known.  Type \002%s%s HELP LEVELS \002 for a list of valid settings."), what.c_str(), Config->UseStrictPrivMsgString.c_str(), source.owner->nick.c_str());

		return;
	}

	void DoList(CommandSource &source, ChannelInfo *ci)
	{
		source.Reply(_("Access level settings for channel %s:"), ci->name.c_str());

		ListFormatter list;
		list.addColumn("Name").addColumn("Level");

		const std::vector<Privilege> &privs = PrivilegeManager::GetPrivileges();

		for (unsigned i = 0; i < privs.size(); ++i)
		{
			const Privilege &p = privs[i];
			int16_t j = ci->GetLevel(p.name);

			ListFormatter::ListEntry entry;
			entry["Name"] = p.name;

			if (j == ACCESS_INVALID)
				entry["Level"] = "(disabled)";
			else if (j == ACCESS_FOUNDER)
				entry["Level"] = "(founder only)";
			else
				entry["Level"] = stringify(j);

			list.addEntry(entry);
		}

		std::vector<Anope::string> replies;
		list.Process(replies);

		for (unsigned i = 0; i < replies.size(); ++i)
			source.Reply(replies[i]);
	}

	void DoReset(CommandSource &source, ChannelInfo *ci)
	{
		User *u = source.u;

		reset_levels(ci);
		FOREACH_MOD(I_OnLevelChange, OnLevelChange(u, ci, "ALL", 0));

		bool override = !ci->AccessFor(u).HasPriv("FOUNDER");
		Log(override ? LOG_OVERRIDE : LOG_COMMAND, u, this, ci) << "to reset all levels";

		source.Reply(_("Access levels for \002%s\002 reset to defaults."), ci->name.c_str());
		return;
	}

 public:
	CommandCSLevels(Module *creator) : Command(creator, "chanserv/levels", 2, 4)
	{
		this->SetDesc(_("Redefine the meanings of access levels"));
		this->SetSyntax(_("\037channel\037 SET \037type\037 \037level\037"));
		this->SetSyntax(_("\037channel\037 {DIS | DISABLE} \037type\037"));
		this->SetSyntax(_("\037channel\037 LIST"));
		this->SetSyntax(_("\037channel\037 RESET"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		const Anope::string &cmd = params[1];
		const Anope::string &what = params.size() > 2 ? params[2] : "";
		const Anope::string &s = params.size() > 3 ? params[3] : "";

		User *u = source.u;

		ChannelInfo *ci = cs_findchan(params[0]);
		if (ci == NULL)
		{
			source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
			return;
		}

		/* If SET, we want two extra parameters; if DIS[ABLE] or FOUNDER, we want only
		 * one; else, we want none.
		 */
		if (cmd.equals_ci("SET") ? s.empty() : (cmd.substr(0, 3).equals_ci("DIS") ? (what.empty() || !s.empty()) : !what.empty()))
			this->OnSyntaxError(source, cmd);
		else if (!ci->AccessFor(u).HasPriv("FOUNDER") && !u->HasPriv("chanserv/access/modify"))
			source.Reply(ACCESS_DENIED);
		else if (cmd.equals_ci("SET"))
			this->DoSet(source, ci, params);
		else if (cmd.equals_ci("DIS") || cmd.equals_ci("DISABLE"))
			this->DoDisable(source, ci, params);
		else if (cmd.equals_ci("LIST"))
			this->DoList(source, ci);
		else if (cmd.equals_ci("RESET"))
			this->DoReset(source, ci);
		else
			this->OnSyntaxError(source, "");

		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		if (subcommand.equals_ci("DESC"))
		{
			source.Reply(_("The following feature/function names are understood."));

			ListFormatter list;
			list.addColumn("Name").addColumn("Description");

			const std::vector<Privilege> &privs = PrivilegeManager::GetPrivileges();
			for (unsigned i = 0; i < privs.size(); ++i)
			{
				const Privilege &p = privs[i];
				ListFormatter::ListEntry entry;
				entry["Name"] = p.name;
				entry["Description"] = translate(source.u, p.desc.c_str());
				list.addEntry(entry);
			}

			std::vector<Anope::string> replies;
			list.Process(replies);

			for (unsigned i = 0; i < replies.size(); ++i)
				source.Reply(replies[i]);
		}
		else
		{
			this->SendSyntax(source);
			source.Reply(" ");
			source.Reply(_("The \002LEVELS\002 command allows fine control over the meaning of\n"
					"the numeric access levels used for channels.  With this\n"
					"command, you can define the access level required for most\n"
					"of %s's functions. (The \002SET FOUNDER\002 and this command\n"
					"are always restricted to the channel founder.)\n"
					" \n"
					"\002LEVELS SET\002 allows the access level for a function or group of\n"
					"functions to be changed. \002LEVELS DISABLE\002 (or \002DIS\002 for short)\n"
					"disables an automatic feature or disallows access to a\n"
					"function by anyone, INCLUDING the founder (although, the founder\n"
					"can always reenable it).\n"
					" \n"
					"\002LEVELS LIST\002 shows the current levels for each function or\n"
					"group of functions. \002LEVELS RESET\002 resets the levels to the\n"
					"default levels of a newly-created channel (see\n"
					"\002HELP ACCESS LEVELS\002).\n"
					" \n"
					"For a list of the features and functions whose levels can be\n"
					"set, see \002HELP LEVELS DESC\002."), source.owner->nick.c_str());
		}
		return true;
	}
};

class CSAccess : public Module
{
	AccessAccessProvider accessprovider;
	CommandCSAccess commandcsaccess;
	CommandCSLevels commandcslevels;

 public:
	CSAccess(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE),
		accessprovider(this), commandcsaccess(this), commandcslevels(this)
	{
		this->SetAuthor("Anope");

		Implementation i[] = { I_OnReload, I_OnCreateChan, I_OnGroupCheckPriv };
		ModuleManager::Attach(i, this, sizeof(i) / sizeof(Implementation));

		try
		{
			this->OnReload();
		}
		catch (const ConfigException &ex)
		{
			throw ModuleException(ex.GetReason());
		}
	}

	void OnReload()
	{
		defaultLevels.clear();
		ConfigReader config;

		for (int i = 0; i < config.Enumerate("privilege"); ++i)
		{
			const Anope::string &pname = config.ReadValue("privilege", "name", "", i);

			Privilege *p = PrivilegeManager::FindPrivilege(pname);
			if (p == NULL)
				continue;

			const Anope::string &value = config.ReadValue("privilege", "level", "", i);
			if (value.empty())
				continue;
			else if (value.equals_ci("founder"))
				defaultLevels[p->name] = ACCESS_FOUNDER;
			else if (value.equals_ci("disabled"))
				defaultLevels[p->name] = ACCESS_INVALID;
			else
				defaultLevels[p->name] = config.ReadInteger("privilege", "level", i, false);
		}
	}

	void OnCreateChan(ChannelInfo *ci)
	{
		reset_levels(ci);
	}

	EventReturn OnGroupCheckPriv(const AccessGroup *group, const Anope::string &priv)
	{
		if (group->ci == NULL)
			return EVENT_CONTINUE;
		/* Special case. Allows a level of < 0 to match anyone, and a level of 0 to match anyone identified. */
		int16_t level = group->ci->GetLevel(priv);
		if (level < 0)
			return EVENT_ALLOW;
		else if (level == 0 && group->nc)
			return EVENT_ALLOW;
		return EVENT_CONTINUE;
	}
};

MODULE_INIT(CSAccess)
