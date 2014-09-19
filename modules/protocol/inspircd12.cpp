/* inspircd 1.2 functions
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

#include "services.h"
#include "modules.h"
#include "nickserv.h"
#include "oper.h"

/* inspircd-ts6.h uses these */
static bool has_globopsmod = false;
static bool has_chghostmod = false;
static bool has_chgidentmod = false;
#include "inspircd-ts6.h"

IRCDVar myIrcd[] = {
	{"InspIRCd 1.2",	/* ircd name */
	 "+I",				/* Modes used by pseudoclients */
	 1,					/* SVSNICK */
	 1,					/* Vhost */
	 0,					/* Supports SNlines */
	 1,					/* Supports SQlines */
	 1,					/* Supports SZlines */
	 0,					/* Join 2 Message */
	 0,					/* Chan SQlines */
	 0,					/* Quit on Kill */
	 1,					/* vidents */
	 1,					/* svshold */
	 0,					/* time stamp on mode */
	 0,					/* O:LINE */
	 1,					/* UMODE */
	 1,					/* No Knock requires +i */
	 0,					/* Can remove User Channel Modes with SVSMODE */
	 0,					/* Sglines are not enforced until user reconnects */
	 1,					/* ts6 */
	 "$",				/* TLD Prefix for Global */
	 20,				/* Max number of modes we can send per line */
	 1,					/* IRCd sends a SSL users certificate fingerprint */
	 }
	,
	{NULL}
};

static bool has_servicesmod = false;
static bool has_svsholdmod = false;
static bool has_hidechansmod = false;

bool event_ftopic(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* :source FTOPIC channel ts setby :topic */
	if (params.size() < 4)
		return true;

	Channel *c = findchan(params[0]);
	if (!c)
	{
		Log(LOG_DEBUG) << "TOPIC " << params[3] << " for nonexistent channel " << params[0];
		return true;
	}

	c->ChangeTopicInternal(params[2], params[3], Anope::string(params[1]).is_pos_number_only() ? convertTo<time_t>(params[1]) : Anope::CurTime);

	return true;
}

bool event_opertype(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* opertype is equivalent to mode +o because servers
	   dont do this directly */
	User *u = finduser(source);
	if (u && !u->HasMode(UMODE_OPER))
	{
		std::vector<Anope::string> newparams;
		newparams.push_back(source);
		newparams.push_back("+o");
		return ircdmessage->OnMode(source, newparams);
	}

	return true;
}

bool event_fmode(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* :source FMODE #test 12345678 +nto foo */
	if (params.size() < 3)
		return true;

	Channel *c = findchan(params[0]);
	if (!c)
		return true;
	
	/* Checking the TS for validity to avoid desyncs */
	time_t ts = Anope::string(params[1]).is_pos_number_only() ? convertTo<time_t>(params[1]) : 0;
	if (c->creation_time > ts)
	{
		/* Our TS is bigger, we should lower it */
		c->creation_time = ts;
		c->Reset();
	}
	else if (c->creation_time < ts)
		/* The TS we got is bigger, we should ignore this message. */
		return true;

	/* TS's are equal now, so we can proceed with parsing */
	std::vector<Anope::string> newparams;
	newparams.push_back(params[0]);
	newparams.push_back(params[1]);
	Anope::string modes = params[2];
	for (unsigned n = 3; n < params.size(); ++n)
		modes += " " + params[n];
	newparams.push_back(modes);

	return ircdmessage->OnMode(source, newparams);
}

bool event_setname(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);
	if (!u)
	{
		Log(LOG_DEBUG) << "SETNAME for nonexistent user " << source;
		return true;
	}

	u->SetRealname(params[0]);
	return true;
}

bool event_chgname(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);

	if (!u)
	{
		Log(LOG_DEBUG) << "FNAME for nonexistent user " << source;
		return true;
	}

	u->SetRealname(params[0]);
	return true;
}

bool event_setident(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);

	if (!u)
	{
		Log(LOG_DEBUG) << "SETIDENT for nonexistent user " << source;
		return true;
	}

	u->SetIdent(params[0]);
	return true;
}

bool event_chgident(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(params[0]);

	if (!u)
	{
		Log(LOG_DEBUG) << "CHGIDENT for nonexistent user " << params[0];
		return true;
	}

	u->SetIdent(params[1]);
	return true;
}

bool event_sethost(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);

	if (!u)
	{
		Log(LOG_DEBUG) << "SETHOST for nonexistent user " << source;
		return true;
	}

	u->SetDisplayedHost(params[0]);
	return true;
}


bool event_chghost(const Anope::string &source, const std::vector<Anope::string> &params)
{
	User *u = finduser(source);

	if (!u)
	{
		Log(LOG_DEBUG) << "FHOST for nonexistent user " << source;
		return true;
	}

	u->SetDisplayedHost(params[0]);
	return true;
}


/*
 *   source     = numeric of the sending server
 *   params[0]  = uuid
 *   params[1]  = metadata name
 *   params[2]  = data
 */
bool event_metadata(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params.size() < 3)
		return true;
	else if (params[1].equals_cs("accountname"))
	{
		User *u = finduser(params[0]);
		NickCore *nc = findcore(params[2]);
		if (u && nc)
		{
			u->Login(nc);
		
			NickAlias *user_na = findnick(u->nick);
			if (!Config->NoNicknameOwnership && nickserv && user_na && user_na->nc == nc && user_na->nc->HasFlag(NI_UNCONFIRMED) == false)
				u->SetMode(findbot(Config->NickServ), UMODE_REGISTERED);
		}
	}

	/*
	 *   possible incoming ssl_cert messages:
	 *   Received: :409 METADATA 409AAAAAA ssl_cert :vTrSe c38070ce96e41cc144ed6590a68d45a6 <...> <...>
	 *   Received: :409 METADATA 409AAAAAC ssl_cert :vTrSE Could not get peer certificate: error:00000000:lib(0):func(0):reason(0)
	 */
	else if (params[1].equals_cs("ssl_cert"))
	{
		User *u = finduser(params[0]);
		if (!u)
			return true;
	 	std::string data = params[2].c_str();
		size_t pos1 = data.find(' ') + 1;
		size_t pos2 = data.find(' ', pos1);
		if ((pos2 - pos1) >= 32) // inspircd supports md5 and sha1 fingerprint hashes -> size 32 or 40 bytes.
		{
			u->fingerprint = data.substr(pos1, pos2 - pos1);
			FOREACH_MOD(I_OnFingerprint, OnFingerprint(u));
		}
	}
	return true;
}

bool event_endburst(const Anope::string &source, const std::vector<Anope::string> &params)
{
	Server *s = Server::Find(source);

	if (!s)
		throw CoreException("Got ENDBURST without a source");

	Log(LOG_DEBUG) << "Processed ENDBURST for " << s->GetName();

	s->Sync(true);
	return true;
}

class InspIRCdExtBan : public ChannelModeList
{
 public:
	InspIRCdExtBan(ChannelModeName mName, char modeChar) : ChannelModeList(mName, modeChar) { }

	bool Matches(User *u, const Entry *e)
	{
		const Anope::string &mask = e->mask;

		if (mask.find("m:") == 0 || mask.find("N:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			Entry en(this->Name, real_mask);
			if (en.Matches(u))
				return true;
		}
		else if (mask.find("j:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			Channel *c = findchan(real_mask);
			if (c != NULL && c->FindUser(u) != NULL)
				return true;
		}
		else if (mask.find("M:") == 0 || mask.find("R:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			if (u->IsIdentified() && real_mask.equals_ci(u->Account()->display))
				return true;
		}
		else if (mask.find("r:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			if (Anope::Match(u->realname, real_mask))
				return true;
		}
		else if (mask.find("s:") == 0)
		{
			Anope::string real_mask = mask.substr(2);

			if (Anope::Match(u->server->GetName(), real_mask))
				return true;
		}

		return false;
	}
};

class Inspircd12IRCdMessage : public InspircdIRCdMessage
{
 public:
	/*
	 * [Nov 03 22:09:58.176252 2009] debug: Received: :964 UID 964AAAAAC 1225746297 w00t2 localhost testnet.user w00t 127.0.0.1 1225746302 +iosw +ACGJKLNOQcdfgjklnoqtx :Robin Burchell <w00t@inspircd.org>
	 * 0: uid
	 * 1: ts
	 * 2: nick
	 * 3: host
	 * 4: dhost
	 * 5: ident
	 * 6: ip
	 * 7: signon
	 * 8+: modes and params -- IMPORTANT, some modes (e.g. +s) may have parameters. So don't assume a fixed position of realname!
	 * last: realname
	 */
	bool OnUID(const Anope::string &source, const std::vector<Anope::string> &params)
	{
		time_t ts = convertTo<time_t>(params[1]);

		Anope::string modes = params[8];
		for (unsigned i = 9; i < params.size() - 1; ++i)
			modes += " " + params[i];
		User *user = do_nick("", params[2], params[5], params[3], source, params[params.size() - 1], ts, params[6], params[4], params[0], modes);
		if (user && user->server->IsSynced() && nickserv)
			nickserv->Validate(user);

		return true;
	}

	bool OnCapab(const Anope::string &source, const std::vector<Anope::string> &params)
	{
		if (params[0].equals_cs("START"))
		{
			/* reset CAPAB */
			has_servicesmod = false;
			has_globopsmod = false;
			has_svsholdmod = false;
			has_chghostmod = false;
			has_chgidentmod = false;
			has_hidechansmod = false;
		}
		else if (params[0].equals_cs("MODULES"))
		{
			if (params[1].find("m_globops.so") != Anope::string::npos)
				has_globopsmod = true;
			if (params[1].find("m_services_account.so") != Anope::string::npos)
				has_servicesmod = true;
			if (params[1].find("m_svshold.so") != Anope::string::npos)
				has_svsholdmod = true;
			if (params[1].find("m_chghost.so") != Anope::string::npos)
				has_chghostmod = true;
			if (params[1].find("m_chgident.so") != Anope::string::npos)
				has_chgidentmod = true;
			if (params[1].find("m_hidechans.so") != Anope::string::npos)
				has_hidechansmod = true;
			if (params[1].find("m_servprotect.so") != Anope::string::npos)
				ircd->pseudoclient_mode = "+Ik";
		}
		else if (params[0].equals_cs("CAPABILITIES"))
		{
			spacesepstream ssep(params[1]);
			Anope::string capab;
			while (ssep.GetToken(capab))
			{
				if (capab.find("CHANMODES") != Anope::string::npos)
				{
					Anope::string modes(capab.begin() + 10, capab.end());
					commasepstream sep(modes);
					Anope::string modebuf;

					sep.GetToken(modebuf);
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						switch (modebuf[t])
						{
							case 'b':
								ModeManager::AddChannelMode(new InspIRCdExtBan(CMODE_BAN, 'b'));
								continue;
							case 'e':
								ModeManager::AddChannelMode(new InspIRCdExtBan(CMODE_EXCEPT, 'e'));
								continue;
							case 'I':
								ModeManager::AddChannelMode(new InspIRCdExtBan(CMODE_INVITEOVERRIDE, 'I'));
								continue;
							/* InspIRCd sends q and a here if they have no prefixes */
							case 'q':
								ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_OWNER, 'q', '@'));
								continue;
							case 'a':
								ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_PROTECT , 'a', '@'));
								continue;
							default:
								ModeManager::AddChannelMode(new ChannelModeList(CMODE_END, modebuf[t]));
						}
					}

					sep.GetToken(modebuf);
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						switch (modebuf[t])
						{
							case 'k':
								ModeManager::AddChannelMode(new ChannelModeKey('k'));
								continue;
							default:
								ModeManager::AddChannelMode(new ChannelModeParam(CMODE_END, modebuf[t]));
						}
					}

					sep.GetToken(modebuf);
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						switch (modebuf[t])
						{
							case 'F':
								ModeManager::AddChannelMode(new ChannelModeParam(CMODE_NICKFLOOD, 'F', true));
								continue;
							case 'J':
								ModeManager::AddChannelMode(new ChannelModeParam(CMODE_NOREJOIN, 'J', true));
								continue;
							case 'L':
								ModeManager::AddChannelMode(new ChannelModeParam(CMODE_REDIRECT, 'L', true));
								continue;
							case 'f':
								ModeManager::AddChannelMode(new ChannelModeFlood('f', true));
								continue;
							case 'j':
								ModeManager::AddChannelMode(new ChannelModeParam(CMODE_JOINFLOOD, 'j', true));
								continue;
							case 'l':
								ModeManager::AddChannelMode(new ChannelModeParam(CMODE_LIMIT, 'l', true));
								continue;
							default:
								ModeManager::AddChannelMode(new ChannelModeParam(CMODE_END, modebuf[t], true));
						}
					}

					sep.GetToken(modebuf);
					for (size_t t = 0, end = modebuf.length(); t < end; ++t)
					{
						switch (modebuf[t])
						{
							case 'A':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_ALLINVITE, 'A'));
								continue;
							case 'B':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_BLOCKCAPS, 'B'));
								continue;
							case 'C':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_NOCTCP, 'C'));
								continue;
							case 'D':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_DELAYEDJOIN, 'D'));
								continue;
							case 'G':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_FILTER, 'G'));
								continue;
							case 'K':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_NOKNOCK, 'K'));
								continue;
							case 'M':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_REGMODERATED, 'M'));
								continue;
							case 'N':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_NONICK, 'N'));
								continue;
							case 'O':
								ModeManager::AddChannelMode(new ChannelModeOper('O'));
								continue;
							case 'P':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_PERM, 'P'));
								continue;
							case 'Q':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_NOKICK, 'Q'));
								continue;
							case 'R':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_REGISTEREDONLY, 'R'));
								continue;
							case 'S':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_STRIPCOLOR, 'S'));
								continue;
							case 'T':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_NONOTICE, 'T'));
								continue;
							case 'c':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_BLOCKCOLOR, 'c'));
								continue;
							case 'i':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_INVITE, 'i'));
								continue;
							case 'm':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_MODERATED, 'm'));
								continue;
							case 'n':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_NOEXTERNAL, 'n'));
								continue;
							case 'p':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_PRIVATE, 'p'));
								continue;
							case 'r':
								ModeManager::AddChannelMode(new ChannelModeRegistered('r'));
								continue;
							case 's':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_SECRET, 's'));
								continue;
							case 't':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_TOPIC, 't'));
								continue;
							case 'u':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_AUDITORIUM, 'u'));
								continue;
							case 'z':
								ModeManager::AddChannelMode(new ChannelMode(CMODE_SSL, 'z'));
								continue;
							default:
								ModeManager::AddChannelMode(new ChannelMode(CMODE_END, modebuf[t]));
						}
					}
				}
				else if (capab.find("USERMODES") != Anope::string::npos)
				{
					Anope::string modes(capab.begin() + 10, capab.end());
					commasepstream sep(modes);
					Anope::string modebuf;

					while (sep.GetToken(modebuf))
					{
						for (size_t t = 0, end = modebuf.length(); t < end; ++t)
						{
							switch (modebuf[t])
							{
								case 'h':
									ModeManager::AddUserMode(new UserMode(UMODE_HELPOP, 'h'));
									continue;
								case 'B':
									ModeManager::AddUserMode(new UserMode(UMODE_BOT, 'B'));
									continue;
								case 'G':
									ModeManager::AddUserMode(new UserMode(UMODE_FILTER, 'G'));
									continue;
								case 'H':
									ModeManager::AddUserMode(new UserMode(UMODE_HIDEOPER, 'H'));
									continue;
								case 'I':
									ModeManager::AddUserMode(new UserMode(UMODE_PRIV, 'I'));
									continue;
								case 'Q':
									ModeManager::AddUserMode(new UserMode(UMODE_HIDDEN, 'Q'));
									continue;
								case 'R':
									ModeManager::AddUserMode(new UserMode(UMODE_REGPRIV, 'R'));
									continue;
								case 'S':
									ModeManager::AddUserMode(new UserMode(UMODE_STRIPCOLOR, 'S'));
									continue;
								case 'W':
									ModeManager::AddUserMode(new UserMode(UMODE_WHOIS, 'W'));
									continue;
								case 'c':
									ModeManager::AddUserMode(new UserMode(UMODE_COMMONCHANS, 'c'));
									continue;
								case 'g':
									ModeManager::AddUserMode(new UserMode(UMODE_CALLERID, 'g'));
									continue;
								case 'i':
									ModeManager::AddUserMode(new UserMode(UMODE_INVIS, 'i'));
									continue;
								case 'k':
									ModeManager::AddUserMode(new UserMode(UMODE_PROTECTED, 'k'));
									continue;
								case 'o':
									ModeManager::AddUserMode(new UserMode(UMODE_OPER, 'o'));
									continue;
								case 'r':
									ModeManager::AddUserMode(new UserMode(UMODE_REGISTERED, 'r'));
									continue;
								case 'w':
									ModeManager::AddUserMode(new UserMode(UMODE_WALLOPS, 'w'));
									continue;
								case 'x':
									ModeManager::AddUserMode(new UserMode(UMODE_CLOAK, 'x'));
									continue;
								case 'd':
									ModeManager::AddUserMode(new UserMode(UMODE_DEAF, 'd'));
									continue;
								default:
									ModeManager::AddUserMode(new UserMode(UMODE_END, modebuf[t]));
							}
						}
					}
				}
				else if (capab.find("PREFIX=(") != Anope::string::npos)
				{
					Anope::string modes(capab.begin() + 8, capab.begin() + capab.find(')'));
					Anope::string chars(capab.begin() + capab.find(')') + 1, capab.end());
					unsigned short level = modes.length() - 1;

					for (size_t t = 0, end = modes.length(); t < end; ++t)
					{
						switch (modes[t])
						{
							case 'q':
								ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_OWNER, 'q', chars[t], level--));
								continue;
							case 'a':
								ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_PROTECT, 'a', chars[t], level--));
								continue;
							case 'o':
								ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_OP, 'o', chars[t], level--));
								continue;
							case 'h':
								ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_HALFOP, 'h', chars[t], level--));
								continue;
							case 'v':
								ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_VOICE, 'v', chars[t], level--));
								continue;
							default:
								ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_END, modes[t], chars[t], level--));
						}
					}
				}
				else if (capab.find("MAXMODES=") != Anope::string::npos)
				{
					Anope::string maxmodes(capab.begin() + 9, capab.end());
					ircd->maxmodes = maxmodes.is_pos_number_only() ? convertTo<unsigned>(maxmodes) : 3;
				}
			}
		}
		else if (params[0].equals_cs("END"))
		{
			if (!has_globopsmod)
			{
				UplinkSocket::Message() << "ERROR :m_globops is not loaded. This is required by Anope";
				quitmsg = "Remote server does not have the m_globops module loaded, and this is required.";
				quitting = true;
				return false;
			}
			if (!has_servicesmod)
			{
				UplinkSocket::Message() << "ERROR :m_services_account.so is not loaded. This is required by Anope";
				quitmsg = "ERROR: Remote server does not have the m_services_account module loaded, and this is required.";
				quitting = true;
				return false;
			}
			if (!has_hidechansmod)
			{
				UplinkSocket::Message() << "ERROR :m_hidechans.so is not loaded. This is required by Anope";
				quitmsg = "ERROR: Remote server does not have the m_hidechans module loaded, and this is required.";
				quitting = true;
				return false;
			}
			if (!has_svsholdmod)
				Log() << "SVSHOLD missing, Usage disabled until module is loaded.";
			if (!has_chghostmod)
				Log() << "CHGHOST missing, Usage disabled until module is loaded.";
			if (!has_chgidentmod)
				Log() << "CHGIDENT missing, Usage disabled until module is loaded.";
			ircd->svshold = has_svsholdmod;
		}

		IRCdMessage::OnCapab(source, params);

		return true;
	}
};

class ProtoInspIRCd : public Module
{
	Message message_endburst, message_time,
		message_rsquit, message_svsmode, message_fhost,
		message_chgident, message_fname, message_sethost, message_setident, message_setname, message_fjoin, message_fmode,
		message_ftopic, message_opertype, message_idle, message_metadata;
	
	InspIRCdTS6Proto ircd_proto;
	Inspircd12IRCdMessage ircd_message;

 public:
	ProtoInspIRCd(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, PROTOCOL),
		message_endburst("ENDBURST", event_endburst),
		message_time("TIME", event_time), message_rsquit("RSQUIT", event_rsquit),
		message_svsmode("SVSMODE", OnMode), message_fhost("FHOST", event_chghost),
		message_chgident("CHGIDENT", event_chgident), message_fname("FNAME", event_chgname),
		message_sethost("SETHOST", event_sethost), message_setident("SETIDENT", event_setident),
		message_setname("SETNAME", event_setname), message_fjoin("FJOIN", OnSJoin),
		message_fmode("FMODE", event_fmode), message_ftopic("FTOPIC", event_ftopic),
		message_opertype("OPERTYPE", event_opertype), message_idle("IDLE", event_idle),
		message_metadata("METADATA", event_metadata)
	{
		this->SetAuthor("Anope");

		pmodule_ircd_var(myIrcd);
		pmodule_ircd_proto(&this->ircd_proto);
		pmodule_ircd_message(&this->ircd_message);

		Capab.insert("NOQUIT");

		Implementation i[] = { I_OnUserNickChange, I_OnServerSync };
		ModuleManager::Attach(i, this, sizeof(i) / sizeof(Implementation));

		if (Config->Numeric.empty())
		{
			Anope::string numeric = ts6_sid_retrieve();
			Me->SetSID(numeric);
			Config->Numeric = numeric;
		}

		for (botinfo_map::iterator it = BotListByNick.begin(), it_end = BotListByNick.end(); it != it_end; ++it)
			it->second->GenerateUID();
	}

	~ProtoInspIRCd()
	{
		pmodule_ircd_var(NULL);
		pmodule_ircd_proto(NULL);
		pmodule_ircd_message(NULL);
	}


	void OnUserNickChange(User *u, const Anope::string &)
	{
		/* InspIRCd 1.2 doesn't set -r on nick change, remove -r here. Note that if we have to set +r later
		 * this will cancel out this -r, resulting in no mode changes.
		 */
		u->RemoveMode(findbot(Config->NickServ), UMODE_REGISTERED);
	}

	void OnServerSync(Server *s)
	{
		if (nickserv)
			for (Anope::insensitive_map<User *>::iterator it = UserListByNick.begin(); it != UserListByNick.end(); ++it)
			{
				User *u = it->second;
				if (u->server == s && !u->IsIdentified())
					nickserv->Validate(u);
			}
	}
};

MODULE_INIT(ProtoInspIRCd)
