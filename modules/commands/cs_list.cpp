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

class CommandCSList : public Command
{
 public:
	CommandCSList(Module *creator) : Command(creator, "chanserv/list", 1, 2)
	{
		this->SetFlag(CFLAG_STRIP_CHANNEL);
		this->SetDesc(_("Lists all registered channels matching the given pattern"));
		this->SetSyntax(_("\037pattern\037"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		Anope::string pattern = params[0];
		unsigned nchans;
		bool is_servadmin = u->HasCommand("chanserv/list");
		int count = 0, from = 0, to = 0;
		bool suspended = false, channoexpire = false;

		if (pattern[0] == '#')
		{
			Anope::string n1 = myStrGetToken(pattern.substr(1), '-', 0), /* Read FROM out */
					n2 = myStrGetTokenRemainder(pattern, '-', 1);

			try
			{
				from = convertTo<int>(n1);
				to = convertTo<int>(n2);
			}
			catch (const ConvertException &)
			{
				source.Reply(LIST_INCORRECT_RANGE);
				source.Reply(_("To search for channels starting with #, search for the channel\n"
					"name without the #-sign prepended (\002anope\002 instead of \002#anope\002)."));
				return;
			}

			pattern = "*";
		}

		nchans = 0;

		if (is_servadmin && params.size() > 1)
		{
			Anope::string keyword;
			spacesepstream keywords(params[1]);
			while (keywords.GetToken(keyword))
			{
				if (keyword.equals_ci("SUSPENDED"))
					suspended = true;
				if (keyword.equals_ci("NOEXPIRE"))
					channoexpire = true;
			}
		}

		Anope::string spattern = "#" + pattern;

		source.Reply(_("List of entries matching \002%s\002:"), pattern.c_str());

		ListFormatter list;
		list.addColumn("Name").addColumn("Description");

		for (registered_channel_map::const_iterator it = RegisteredChannelList.begin(), it_end = RegisteredChannelList.end(); it != it_end; ++it)
		{
			ChannelInfo *ci = it->second;

			if (!is_servadmin && (ci->HasFlag(CI_PRIVATE) || ci->HasFlag(CI_SUSPENDED)))
				continue;
			else if (suspended && !ci->HasFlag(CI_SUSPENDED))
				continue;
			else if (channoexpire && !ci->HasFlag(CI_NO_EXPIRE))
				continue;

			if (pattern.equals_ci(ci->name) || ci->name.equals_ci(spattern) || Anope::Match(ci->name, pattern) || Anope::Match(ci->name, spattern))
			{
				if (((count + 1 >= from && count + 1 <= to) || (!from && !to)) && ++nchans <= Config->CSListMax)
				{
					bool isnoexpire = false;
					if (is_servadmin && (ci->HasFlag(CI_NO_EXPIRE)))
						isnoexpire = true;

					ListFormatter::ListEntry entry;
					entry["Name"] = (isnoexpire ? "!" : "") + ci->name;
					if (ci->HasFlag(CI_SUSPENDED))
						entry["Description"] = "[Suspended]";
					else
						entry["Description"] = ci->desc;
					list.addEntry(entry);
				}
				++count;
			}
		}

		std::vector<Anope::string> replies;
		list.Process(replies);

		for (unsigned i = 0; i < replies.size(); ++i)
			source.Reply(replies[i]);

		source.Reply(_("End of list - %d/%d matches shown."), nchans > Config->CSListMax ? Config->CSListMax : nchans, nchans);
		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		this->SendSyntax(source);
		source.Reply(" ");
		source.Reply(_("Lists all registered channels matching the given pattern.\n"
				"(Channels with the \002PRIVATE\002 option set are not listed.)\n"
				"Note that a preceding '#' specifies a range, channel names\n"
				"are to be written without '#'."));
		return true;
	}
};

class CSList : public Module
{
	CommandCSList commandcslist;

 public:
	CSList(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, CORE), commandcslist(this)
	{
		this->SetAuthor("Anope");

	}
};

MODULE_INIT(CSList)
