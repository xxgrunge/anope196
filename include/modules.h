/* Modular support
 *
 * (C) 2003-2012 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#ifndef MODULES_H
#define MODULES_H

#include <time.h>
#include "services.h"
#include <stdio.h>
#include "timers.h"
#include "hashcomp.h"

/** Possible return types from events.
 */
enum EventReturn
{
	EVENT_STOP,
	EVENT_CONTINUE,
	EVENT_ALLOW
};

/**
 * This #define allows us to call a method in all
 * loaded modules in a readable simple way, e.g.:
 * 'FOREACH_MOD(I_OnConnect,OnConnect(user));'
 */
#define FOREACH_MOD(y, x) \
if (true) \
{ \
	std::vector<Module*>::iterator safei; \
	for (std::vector<Module*>::iterator _i = ModuleManager::EventHandlers[y].begin(); _i != ModuleManager::EventHandlers[y].end(); ) \
	{ \
		safei = _i; \
		++safei; \
		try \
		{ \
			(*_i)->x ; \
		} \
		catch (const ModuleException &modexcept) \
		{ \
			Log() << "Exception caught: " << modexcept.GetReason(); \
		} \
		_i = safei; \
	} \
} \
else \
	static_cast<void>(0)

/**
 * This define is similar to the one above but returns a result in MOD_RESULT.
 * The first module to return a nonzero result is the value to be accepted,
 * and any modules after are ignored.
 */
#define FOREACH_RESULT(y, x) \
if (true) \
{ \
	std::vector<Module*>::iterator safei; \
	MOD_RESULT = EVENT_CONTINUE; \
	for (std::vector<Module*>::iterator _i = ModuleManager::EventHandlers[y].begin(); _i != ModuleManager::EventHandlers[y].end(); ) \
	{ \
		safei = _i; \
		++safei; \
		try \
		{ \
			EventReturn res = (*_i)->x ; \
			if (res != EVENT_CONTINUE) { \
				MOD_RESULT = res; \
				break; \
			} \
		} \
		catch (const ModuleException &modexcept) \
		{ \
			Log() << "Exception caught: " << modexcept.GetReason(); \
		} \
		_i = safei; \
	} \
} \
else \
	static_cast<void>(0)

#ifndef _WIN32
# include <dlfcn.h>
	/* Define these for systems without them */
# ifndef RTLD_NOW
#  define RTLD_NOW 0
# endif
# ifndef RTLD_LAZY
#  define RTLD_LAZY RTLD_NOW
# endif
# ifndef RTLD_GLOBAL
#  define RTLD_GLOBAL 0
# endif
# ifndef RTLD_LOCAL
#  define RTLD_LOCAL 0
# endif
#endif

class Message;

enum ModuleReturn
{
	MOD_ERR_OK,
	MOD_ERR_MEMORY,
	MOD_ERR_PARAMS,
	MOD_ERR_EXISTS,
	MOD_ERR_NOEXIST,
	MOD_ERR_NOLOAD,
	MOD_ERR_UNKNOWN,
	MOD_ERR_FILE_IO,
	MOD_ERR_EXCEPTION,
	MOD_ERR_VERSION
};

/** Priority types which can be returned from Module::Prioritize()
 */
enum Priority { PRIORITY_FIRST, PRIORITY_DONTCARE, PRIORITY_LAST, PRIORITY_BEFORE, PRIORITY_AFTER };
/* Module types, in the order in which they are unloaded. The order these are in is IMPORTANT */
enum ModType { MT_BEGIN, THIRD, SUPPORTED, CORE, DATABASE, ENCRYPTION, PROTOCOL, MT_END };

typedef std::multimap<Anope::string, Message *> message_map;
extern CoreExport message_map MessageMap;
class Module;
extern CoreExport std::list<Module *> Modules;

class Version
{
 private:
	int Major;
	int Minor;
	int Build;

 public:
	/** Constructor
	 * @param vMajor The major version numbber
	 * @param vMinor The minor version numbber
	 * @param vBuild The build version numbber
	 */
	Version(int vMajor, int vMinor, int vBuild);

	/** Destructor
	 */
	virtual ~Version();

	/** Get the major version of Anope this was built against
	 * @return The major version
	 */
	int GetMajor() const;

	/** Get the minor version of Anope this was built against
	 * @return The minor version
	 */
	int GetMinor() const;

	/** Get the build version this was built against
	 * @return The build version
	 */
	int GetBuild() const;
};

class CallBack;
class XLineManager;
struct CommandSource;

/** Every module in Anope is actually a class.
 */
class CoreExport Module : public Extensible
{
 private:
	bool permanent;
 public:
	/** The module name (e.g. os_modload)
	 */
	Anope::string name;

	/** What type this module is
	 */
	ModType type;

	/** The temporary path/filename
	 */
	Anope::string filename;

	/** Callbacks used in this module
	 */
	std::list<CallBack *> CallBacks;

	/** Handle for this module, obtained from dlopen()
	 */
	void *handle;

	/** Time this module was created
	 */
	time_t created;

	/** Version of this module
	 */
	Anope::string version;

	/** Author of the module
	 */
	Anope::string author;

	/** Creates and initialises a new module.
	 * @param modname The module name
	 * @param loadernick The nickname of the user loading the module.
	 * @param type The module type
	 */
	Module(const Anope::string &modname, const Anope::string &loadernick, ModType type = THIRD);

	/** Destroys a module, freeing resources it has allocated.
	 */
	virtual ~Module();

	/** Toggles the permanent flag on a module. If a module is permanent,
	 * then it may not be unloaded.
	 *
	 * Naturally, this setting should be used sparingly!
	 *
	 * @param state True if this module should be permanent, false else.
	 */
	void SetPermanent(bool state);

	/** Retrieves whether or not a given module is permanent.
	 * @return true if the module is permanent, false else.
	 */
	bool GetPermanent() const;

	/** Set the modules version info.
	 * @param version the version of the module
	 */
	void SetVersion(const Anope::string &version);

	/** Set the modules author info
	 * @param author the author of the module
	 */
	void SetAuthor(const Anope::string &author);

	/** Get the version of Anope this module was
	 * compiled against
	 * @return The version
	 */
	Version GetVersion() const;

	/** Called when the ircd notifies that a user has been kicked from a channel.
	 * @param c The channel the user has been kicked from.
	 * @param target The user that has been kicked.
	 * @param source The nick of the sender.
	 * @param kickmsg The reason for the kick.
	 */
	virtual void OnUserKicked(Channel *c, User *target, const Anope::string &source, const Anope::string &kickmsg) { }

	/** Called when Services' configuration has been loaded.
	 */
	virtual void OnReload() { }

	/** Called before a bot is assigned to a channel.
	 * @param sender The user assigning the bot
	 * @param ci The channel the bot is to be assigned to.
	 * @param bi The bot being assigned.
	 * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to deny the assign.
	 */
	virtual EventReturn OnBotAssign(User *sender, ChannelInfo *ci, BotInfo *bi) { return EVENT_CONTINUE; }

	/** Called before a bot is unassigned from a channel.
	 * @param sender The user unassigning the bot
	 * @param ci The channel the bot is being removed from
	 * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to deny the unassign.
	 */
	virtual EventReturn OnBotUnAssign(User *sender, ChannelInfo *ci) { return EVENT_CONTINUE; }

	/** Called when a new user connects to the network.
	 * @param u The connecting user.
	 * @param exempt set to true/is true if the user should be excepted from bans etc
	 */
	virtual void OnUserConnect(dynamic_reference<User> &u, bool &exempt) { }

	/** Called when a new server connects to the network.
	 * @param s The server that has connected to the network
	 */
	virtual void OnNewServer(Server *s) { }

	/** Called after a user changed the nick
	 * @param u The user.
	 * @param oldnick The old nick of the user
	 */
	virtual void OnUserNickChange(User *u, const Anope::string &oldnick) { }

	/** Called when someone uses the generic/help command
	 * @param source Command source
	 * @param params Params
	 * @return EVENT_STOP to stop processing
	 */
	virtual EventReturn OnPreHelp(CommandSource &source, const std::vector<Anope::string> &params) { return EVENT_CONTINUE; }

	/** Called when someone uses the generic/help command
	 * @param source Command source
	 * @param params Params
	 */
	virtual void OnPostHelp(CommandSource &source, const std::vector<Anope::string> &params) { }

	/** Called before a command is due to be executed.
	 * @param source The source of the command
	 * @param command The command the user is executing
	 * @param params The parameters the user is sending
	 * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to halt the command and not process it
	 */
	virtual EventReturn OnPreCommand(CommandSource &source, Command *command, std::vector<Anope::string> &params) { return EVENT_CONTINUE; }

	/** Called after a command has been executed.
	 * @param source The source of the command
	 * @param command The command the user executed
	 * @param params The parameters the user sent
	 */
	virtual void OnPostCommand(CommandSource &source, Command *command, const std::vector<Anope::string> &params) { }

	/** Called when the databases are saved
	 * @return EVENT_CONTINUE to let other modules continue saving, EVENT_STOP to stop
	 */
	virtual EventReturn OnSaveDatabase() { return EVENT_CONTINUE; }

	/** Called when the databases are loaded
	 * @return EVENT_CONTINUE to let other modules continue saving, EVENT_STOP to stop
	 */
	virtual EventReturn OnLoadDatabase() { return EVENT_CONTINUE; }

	/** Called when anope needs to check passwords against encryption
	 *  see src/encrypt.c for detailed informations
	 */
	virtual EventReturn OnEncrypt(const Anope::string &src, Anope::string &dest) { return EVENT_CONTINUE; }
	virtual EventReturn OnDecrypt(const Anope::string &hashm, const Anope::string &src, Anope::string &dest) { return EVENT_CONTINUE; }

	/** Called on fantasy command
	 * @param command The command
	 * @param u The user using the command
	 * @param ci The channel it's being used in
	 * @param params The params
	 */
	virtual void OnBotFantasy(const Anope::string &command, User *u, ChannelInfo *ci, const Anope::string &params) { }

	/** Called on fantasy command without access
	 * @param command The command
	 * @param u The user using the command
	 * @param ci The channel it's being used in
	 * @param params The params
	 */
	virtual void OnBotNoFantasyAccess(const Anope::string &command, User *u, ChannelInfo *ci, const Anope::string &params) { }

	/** Called after a bot joins a channel
	 * @param c The channel
	 * @param bi The bot
	 */
	virtual void OnBotJoin(Channel *c, BotInfo *bi) { }

	/** Called when a bot places a ban
	 * @param u User being banned
	 * @param ci Channel the ban is placed on
	 * @param mask The mask being banned
	 */
	virtual void OnBotBan(User *u, ChannelInfo *ci, const Anope::string &mask) { }

	/** Called before a badword is added to the badword list
	 * @param ci The channel
	 * @param bw The badword
	 */
	virtual void OnBadWordAdd(ChannelInfo *ci, BadWord *bw) { }

	/** Called before a badword is deleted from a channel
	 * @param ci The channel
	 * @param bw The badword
	 */
	virtual void OnBadWordDel(ChannelInfo *ci, BadWord *bw) { }

	/** Called in findbot()
	 * @param nick The nick being looked up
	 */
	virtual void OnFindBot(const Anope::string &nick) { }

	/** Called before a bot kicks a user
	 * @param bi The bot sending the kick
	 * @param c The channel the user is being kicked on
	 * @param u The user being kicked
	 * @param reason The reason
	 * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to halt the command and not process it
	 */
	virtual EventReturn OnBotKick(BotInfo *bi, Channel *c, User *u, const Anope::string &reason) { return EVENT_CONTINUE; }

	/** Called before a user parts a channel
	 * @param u The user
	 * @param c The channel
	 */
	virtual void OnPrePartChannel(User *u, Channel *c) {}

	/** Called when a user parts a channel
	 * @param u The user
	 * @param c The channel, may be NULL if the channel no longer exists
	 * @param channel The channel name
	 * @param msg The part reason
	 */
	virtual void OnPartChannel(User *u, Channel *c, const Anope::string &channel, const Anope::string &msg) { }

	/** Called when a user leaves a channel.
	 * From either parting, being kicked, or quitting/killed!
	 * @param u The user
	 * @param c The channel
	 */
	virtual void OnLeaveChannel(User *u, Channel *c) { }

	/** Called before a user joins a channel
	 * @param u The user
	 * @param c The channel
	 * @return EVENT_STOP to allow the user to join the channel through restrictions, EVENT_CONTINUE to let other modules decide
	 */
	virtual EventReturn OnPreJoinChannel(User *u, Channel *c) { return EVENT_CONTINUE; }

	/** Called when a user joins a channel
	 * @param u The user
	 * @param channel The channel
	 */
	virtual void OnJoinChannel(User *u, Channel *c) { }

	/** Called when a new topic is set
	 * @param c The channel
	 * @param topic The new topic
	 */
	virtual void OnTopicUpdated(Channel *c, const Anope::string &topic) { }

	/** Called before a channel expires
	 * @param ci The channel
	 * @param expire Set to true to allow the chan to expire
	 */
	virtual void OnPreChanExpire(ChannelInfo *ci, bool &expire) { }

	/** Called before a channel expires
	 * @param ci The channel
	 */
	virtual void OnChanExpire(ChannelInfo *ci) { }

	/** Called before Anope connecs to its uplink
	 */
	virtual void OnPreServerConnect() { }

	/** Called when Anope connects to its uplink
	 */
	virtual void OnServerConnect() { }

	/** Called when we are almost done synching with the uplink, just before we send the EOB
	 */
	virtual void OnPreUplinkSync(Server *serv) { }

	/** Called when Anope disconnects from its uplink, before it tries to reconnect
	 */
	virtual void OnServerDisconnect() { }

	/** Called when services restart
	*/
	virtual void OnRestart() { }

	/** Called when services shutdown
	 */
	virtual void OnShutdown() { }

	/** Called before a nick expires
	 * @param na The nick
	 * @param expire Set to true to allow the nick to expire
	 */
	virtual void OnPreNickExpire(NickAlias *na, bool &expire) { }

	/** Called when a nick drops
	 * @param na The nick
	 */
	virtual void OnNickExpire(NickAlias *na) { }

	/** Called when defcon level changes
	 * @param level The level
	 */
	virtual void OnDefconLevel(int level) { }

	/** Called after an exception has been added
	 * @param ex The exception
	 * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to halt the command and not process it
	 */
	virtual EventReturn OnExceptionAdd(Exception *ex) { return EVENT_CONTINUE; }

	/** Called before an exception is deleted
	 * @param u The user who is deleting it
	 * @param ex The exceotion
	 */
	virtual void OnExceptionDel(User *u, Exception *ex) { }

	/** Called before a XLine is added
	 * @param u The user adding the XLine
	 * @param x The XLine
	 * @param xlm The xline manager it was added to
	 * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to halt the command and not process it
	 */
	virtual EventReturn OnAddXLine(User *u, XLine *x, XLineManager *xlm) { return EVENT_CONTINUE; }

	/** Called before a XLine is deleted
	 * @param u The user deleting the XLine
	 * @param x The XLine, can be NULL for all XLines
	 * @param xlm The xline manager it was deleted from
	 */
	virtual void OnDelXLine(User *u, XLine *x, XLineManager *xlm) { }

	/** Called when a user is checked for whether they are a services oper
	 * @param u The user
	 * @return EVENT_ALLOW to allow, anything else to deny
	 */
	virtual EventReturn IsServicesOper(User *u) { return EVENT_CONTINUE; }

	/** Called when a server quits
	 * @param server The server
	 */
	virtual void OnServerQuit(Server *server) { }

	/** Called on a QUIT
	 * @param u The user
	 * @param msg The quit message
	 */
	virtual void OnUserQuit(User *u, const Anope::string &msg) { }

	/** Called when a user disconnects
	 * @param u The user
	 */
	virtual void OnUserLogoff(User *u) { }

	/** Called when a new bot is made
	 * @param bi The bot
	 */
	virtual void OnBotCreate(BotInfo *bi) { }

	/** Called when a bot is changed
	 * @param bi The bot
	 */
	virtual void OnBotChange(BotInfo *bi) { }

	/** Called when a bot is deleted
	 * @param bi The bot
	 */
	virtual void OnBotDelete(BotInfo *bi) { }

	/** Called when access is deleted from a channel
	 * @param ci The channel
	 * @param u The user who removed the access
	 * @param access The access entry being removed
	 */
	virtual void OnAccessDel(ChannelInfo *ci, User *u, ChanAccess *access) { }

	/** Called when access is added
	 * @param ci The channel
	 * @param u The user who added the access
	 * @param access The access changed
	 */
	virtual void OnAccessAdd(ChannelInfo *ci, User *u, ChanAccess *access) { }

	/** Called when the access list is cleared
	 * @param ci The channel
	 * @param u The user who cleared the access
	 */
	virtual void OnAccessClear(ChannelInfo *ci, User *u) { }

	/** Called when a level for a channel is changed
	 * @param u The user changing the level
	 * @param ci The channel the level was changed on
	 * @param priv The privilege changed
	 * @param what The new level
	 */
	virtual void OnLevelChange(User *u, ChannelInfo *ci, const Anope::string &priv, int16_t what) { }

	/** Called when a channel is dropped
	 * @param chname The channel name
	 */
	virtual void OnChanDrop(const Anope::string &chname) { }

	/** Called when a channel is registered
	 * @param ci The channel
	 */
	virtual void OnChanRegistered(ChannelInfo *ci) { }

	/** Called when a channel is suspended
	 * @param ci The channel
	 */
	virtual void OnChanSuspend(ChannelInfo *ci) { }

	/** Called when a channel is unsuspended
	 * @param ci The channel
	 */
	virtual void OnChanUnsuspend(ChannelInfo *ci) { }

	/** Called when a channel is being created, for any reason
	 * @param ci The channel
	 */
	virtual void OnCreateChan(ChannelInfo *ci) { }

	/** Called when a channel is being deleted, for any reason
	 * @param ci The channel
	 */
	virtual void OnDelChan(ChannelInfo *ci) { }

	/** Called when a new channel is created
	 * @param c The channel
	 */
	virtual void OnChannelCreate(Channel *c) { }

	/** Called when a channel is deleted
	 * @param c The channel
	 */
	virtual void OnChannelDelete(Channel *c) { }

	/** Called after adding an akick to a channel
	 * @param u The user adding the akick
	 * @param ci The channel
	 * @param ak The akick
	 */
	virtual void OnAkickAdd(User *u, ChannelInfo *ci, AutoKick *ak) { }

	/** Called before removing an akick from a channel
	 * @param u The user removing the akick
	 * @param ci The channel
	 * @param ak The akick
	 */
	virtual void OnAkickDel(User *u, ChannelInfo *ci, AutoKick *ak) { }

	/** Called after a user join a channel when we decide whether to kick them or not
	 * @param u The user
	 * @param ci The channel
	 * @param kick Set to true to kick
	 * @return EVENT_ALLOW to stop processing immediatly
	 */
	virtual EventReturn OnCheckKick(User *u, ChannelInfo *ci, bool &kick) { return EVENT_CONTINUE; }

	/** Called when a user requests info for a channel
	 * @param source The user requesting info
	 * @param ci The channel the user is requesting info for
	 * @param info Data to show the user requesting information
	 * @param ShowHidden true if we should show the user everything
	 */
	virtual void OnChanInfo(CommandSource &source, ChannelInfo *ci, InfoFormatter &info, bool ShowHidden) { }

	/** Called on cs_findchan()
	 * @param chname The name being looked up
	 */
	virtual void OnFindChan(const Anope::string &chname) { }

	/** Checks if access has the channel privilege 'priv'.
	 * @param access THe access struct
	 * @param priv The privilege being checked for
	 * @return EVENT_ALLOW for yes, EVENT_STOP to stop all processing
	 */
	virtual EventReturn OnCheckPriv(ChanAccess *access, const Anope::string &priv) { return EVENT_CONTINUE; }

	/** Check whether an access group has a privilege
	 * @param group The group
	 * @param priv The privilege
	 * @return MOD_ALLOW to allow, MOD_STOP to stop
	 */
	virtual EventReturn OnGroupCheckPriv(const AccessGroup *group, const Anope::string &priv) { return EVENT_CONTINUE; }

	/** Called when a nick is dropped
	 * @param u The user dropping the nick
	 * @param na The nick
	 */
	virtual void OnNickDrop(User *u, NickAlias *na) { }

	/** Called when a nick is forbidden
	 * @param na The nick alias of the forbidden nick
	 */
	virtual void OnNickForbidden(NickAlias *na) { }

	/** Called when a user groups their nick
	 * @param u The user grouping
	 * @param target The target they're grouping to
	 */
	virtual void OnNickGroup(User *u, NickAlias *target) { }

	/** Called when a user identifies
	 * @param u The user
	 */
	virtual void OnNickIdentify(User *u) { }

	/** Called when a nick logs out
	 * @param u The nick
	 */
	virtual void OnNickLogout(User *u) { }

	/** Called when a nick is registered
	 * @param The nick
	 */
	virtual void OnNickRegister(NickAlias *na) { }

	/** Called when a nick is suspended
	 * @param na The nick alias
	 */
	virtual void OnNickSuspend(NickAlias *na) { }

	/** Called when a nick is unsuspneded
	 * @param na The nick alias
	 */
	virtual void OnNickUnsuspended(NickAlias *na) { }

	/** Called on delnick()
	 * @ param na pointer to the nickalias
	 */
	virtual void OnDelNick(NickAlias *na) { }

	/** Called on delcore()
	 * @param nc pointer to the NickCore
	 */
	virtual void OnDelCore(NickCore *nc) { }

	/** Called on change_core_display()
	 * @param nc pointer to the NickCore
	 * @param newdisplay the new display
	 */
	virtual void OnChangeCoreDisplay(NickCore *nc, const Anope::string &newdisplay) { }

	/** called from NickCore::ClearAccess()
	 * @param nc pointer to the NickCore
	 */
	virtual void OnNickClearAccess(NickCore *nc) { }

	/** Called when a user adds an entry to their access list
	 * @param nc The nick
	 * @param entry The entry
	 */
	virtual void OnNickAddAccess(NickCore *nc, const Anope::string &entry) { }

	/** Called from NickCore::EraseAccess()
	 * @param nc pointer to the NickCore
	 * @param entry The access mask
	 */
	virtual void OnNickEraseAccess(NickCore *nc, const Anope::string &entry) { }

	/** called from NickCore::ClearCert()
	 * @param nc pointer to the NickCore
	 */
	virtual void OnNickClearCert(NickCore *nc) { }

	/** Called when a user adds an entry to their cert list
	 * @param nc The nick
	 * @param entry The entry
	 */
	virtual void OnNickAddCert(NickCore *nc, const Anope::string &entry) { }

	/** Called from NickCore::EraseCert()
	 * @param nc pointer to the NickCore
	 * @param entry The fingerprint
	 */
	virtual void OnNickEraseCert(NickCore *nc, const Anope::string &entry) { }

	/** Called when a user requests info for a nick
	 * @param source The user requesting info
	 * @param na The nick the user is requesting info from
	 * @param info Data to show the user requesting information
	 * @param ShowHidden true if we should show the user everything
	 */
	virtual void OnNickInfo(CommandSource &source, NickAlias *na, InfoFormatter &info, bool ShowHidden) { }

	/** Called in findnick()
	 * Useful to modify the na returned by findnick()
	 * @param nick The nick being looked up
	 */
	virtual void OnFindNick(const Anope::string &nick) { }

	/** Called in findcore()
	 * Useful to modify the nc returned by findcore()
	 * @param nick The nick being looked up
	 */
	virtual void OnFindCore(const Anope::string &nick) { }

	/** Check whether a users password is correct.
	 * @param u The user
	 * @param command The command the user is doing
	 * @param params Command params
	 * @param account The account the password should be checked against
	 * @param password The password
	 * @return EVENT_ALLOW to allow the password, EVENT_STOP to stop processing completely
	 */
	virtual EventReturn OnCheckAuthentication(Command *c, CommandSource *source, const std::vector<Anope::string> &params, const Anope::string &account, const Anope::string &password) { return EVENT_CONTINUE; }

	/** Called when a user does /ns update
	 * @param u The user
	 */
	virtual void OnNickUpdate(User *u) { }

	/** Called when we get informed about a users SSL fingerprint
	 *  when we call this, the fingerprint should already be stored in the user struct
	 * @param u pointer to the user
	 */
	virtual void OnFingerprint(User *u) { }

	/** Called when a user becomes (un)away
	 * @param message The message, is .empty() if unaway
	 */
	virtual void OnUserAway(User *u, const Anope::string &message) { }

	/** Called when a vhost is deleted
	 * @param na The nickalias of the vhost
	 */
	virtual void OnDeleteVhost(NickAlias *na) { }

	/** Called when a vhost is set
	 * @param na The nickalias of the vhost
	 */
	virtual void OnSetVhost(NickAlias *na) { }

	/** Called when a memo is sent
	 * @param source The source of the memo
	 * @param target The target of the memo
	 * @param mi Memo info for target
	 * @param m The memo
	 */
	virtual void OnMemoSend(const Anope::string &source, const Anope::string &target, MemoInfo *mi, Memo *m) { }

	/** Called when a memo is deleted
	 * @param nc The nickcore of the memo being deleted
	 * @param mi The memo info
	 * @param m The memo
	 * @param number What memo number is being deleted, can be 0 for all memos
	 */
	virtual void OnMemoDel(const NickCore *nc, MemoInfo *mi, Memo *m) { }

	/** Called when a memo is deleted
	 * @param ci The channel of the memo being deleted
	 * @param mi The memo info
	 * @param m The memo
	 * @param number What memo number is being deleted, can be 0 for all memos
	 */
	virtual void OnMemoDel(ChannelInfo *ci, MemoInfo *mi, Memo *m) { }

	/** Called when a mode is set on a channel
	 * @param c The channel
	 * @param Name The mode name
	 * @param param The mode param, if there is one
	 * @return EVENT_STOP to make mlock/secureops etc checks not happen
	 */
	virtual EventReturn OnChannelModeSet(Channel *c, ChannelModeName Name, const Anope::string &param) { return EVENT_CONTINUE; }

	/** Called when a mode is unset on a channel
	 * @param c The channel
	 * @param Name The mode name
	 * @param param The mode param, if there is one
	 * @return EVENT_STOP to make mlock/secureops etc checks not happen
	 */
	virtual EventReturn OnChannelModeUnset(Channel *c, ChannelModeName Name, const Anope::string &param) { return EVENT_CONTINUE; }

	/** Called when a mode is set on a user
	 * @param u The user
	 * @param Name The mode name
	 */
	virtual void OnUserModeSet(User *u, UserModeName Name) { }

	/** Called when a mode is unset from a user
	 * @param u The user
	 * @param Name The mode name
	 */
	virtual void OnUserModeUnset(User *u, UserModeName Name) { }

	/** Called when a channel mode is introducted into Anope
	 * @param cm The mode
	 */
	virtual void OnChannelModeAdd(ChannelMode *cm) { }

	/** Called when a user mode is introducted into Anope
	 * @param um The mode
	 */
	virtual void OnUserModeAdd(UserMode *um) { }

	/** Called when a mode is about to be mlocked
	 * @param ci The channel the mode is being locked on
	 * @param lock The mode lock
	 * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to deny the mlock.
	 */
	virtual EventReturn OnMLock(ChannelInfo *ci, ModeLock *lock) { return EVENT_CONTINUE; }

	/** Called when a mode is about to be unlocked
	 * @param ci The channel the mode is being unlocked from
	 * @param lock The mode lock
	 * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to deny the mlock.
	 */
	virtual EventReturn OnUnMLock(ChannelInfo *ci, ModeLock *lock) { return EVENT_CONTINUE; }

	/** Called after a module is loaded
	 * @param u The user loading the module, can be NULL
	 * @param m The module
	 */
	virtual void OnModuleLoad(User *u, Module *m) { }

	/** Called before a module is unloaded
	 * @param u The user, can be NULL
	 * @param m The module
	 */
	virtual void OnModuleUnload(User *u, Module *m) { }

	/** Called when a server is synced
	 * @param s The server, can be our uplink server
	 */
	virtual void OnServerSync(Server *s) { }

	/** Called when we sync with our uplink
	 * @param s Our uplink
	 */
	virtual void OnUplinkSync(Server *s) { }

	/** Called when we receive a PRIVMSG for one of our clients
	 * @param u The user sending the PRIVMSG
	 * @param bi The target of the PRIVMSG
	 * @param message The message
	 * @return EVENT_STOP to halt processing
	 */
	virtual EventReturn OnBotPrivmsg(User *u, BotInfo *bi, Anope::string &message) { return EVENT_CONTINUE; }

	/** Called when we receive a PRIVMSG for a registered channel we are in
	 * @param u The source of the message
	 * @param c The channel
	 * @param msg The message
	 */
	virtual void OnPrivmsg(User *u, Channel *c, Anope::string &msg) { }

	/** Called when a message is logged
	 * @param l The log message
	 */
	virtual void OnLog(Log *l) { }
};

/** Implementation-specific flags which may be set in ModuleManager::Attach()
 */
enum Implementation
{
	I_BEGIN,
		/* NickServ */
		I_OnPreNickExpire, I_OnNickExpire, I_OnNickForbidden, I_OnNickGroup, I_OnNickLogout, I_OnNickIdentify, I_OnNickDrop,
		I_OnNickRegister, I_OnNickSuspended, I_OnNickUnsuspended,
		I_OnDelNick, I_OnDelCore, I_OnChangeCoreDisplay,
		I_OnNickClearAccess, I_OnNickAddAccess, I_OnNickEraseAccess,
		I_OnNickClearCert, I_OnNickAddCert, I_OnNickEraseCert,
		I_OnNickInfo, I_OnFindNick, I_OnFindCore, I_OnCheckAuthentication,
		I_OnNickUpdate,

		/* ChanServ */
		I_OnChanSuspend, I_OnChanDrop, I_OnPreChanExpire, I_OnChanExpire, I_OnAccessAdd,
		I_OnAccessDel, I_OnAccessClear, I_OnLevelChange, I_OnChanRegistered, I_OnChanUnsuspend, I_OnCreateChan, I_OnDelChan, I_OnChannelCreate,
		I_OnChannelDelete, I_OnAkickAdd, I_OnAkickDel, I_OnCheckKick,
		I_OnChanInfo, I_OnFindChan, I_OnCheckPriv, I_OnGroupCheckPriv,

		/* BotServ */
		I_OnBotJoin, I_OnBotKick, I_OnBotCreate, I_OnBotChange, I_OnBotDelete, I_OnBotAssign, I_OnBotUnAssign,
		I_OnUserKicked, I_OnBotFantasy, I_OnBotNoFantasyAccess, I_OnBotBan, I_OnBadWordAdd, I_OnBadWordDel, I_OnFindBot,

		/* HostServ */
		I_OnSetVhost, I_OnDeleteVhost,

		/* MemoServ */
		I_OnMemoSend, I_OnMemoDel,

		/* Users */
		I_OnUserConnect, I_OnUserNickChange, I_OnUserQuit, I_OnUserLogoff, I_OnPreJoinChannel,
		I_OnJoinChannel, I_OnPrePartChannel, I_OnPartChannel, I_OnLeaveChannel, I_OnFingerprint, I_OnUserAway,

		/* OperServ */
		I_OnDefconLevel, I_OnAddAkill, I_OnDelAkill, I_OnExceptionAdd, I_OnExceptionDel,
		I_OnAddXLine, I_OnDelXLine, I_IsServicesOper,

		/* Database */
		I_OnSaveDatabase, I_OnLoadDatabase,

		/* Modules */
		I_OnModuleLoad, I_OnModuleUnload,

		/* Other */
		I_OnReload, I_OnNewServer, I_OnPreServerConnect, I_OnServerConnect, I_OnPreUplinkSync, I_OnServerDisconnect,
		I_OnPreHelp, I_OnPostHelp, I_OnPreCommand, I_OnPostCommand, I_OnRestart, I_OnShutdown,
		I_OnServerQuit, I_OnTopicUpdated,
		I_OnEncrypt, I_OnDecrypt,
		I_OnChannelModeSet, I_OnChannelModeUnset, I_OnUserModeSet, I_OnUserModeUnset, I_OnChannelModeAdd, I_OnUserModeAdd,
		I_OnMLock, I_OnUnMLock, I_OnServerSync, I_OnUplinkSync, I_OnBotPrivmsg, I_OnPrivmsg, I_OnLog,
	I_END
};

/** Used to manage modules.
 */
class CoreExport ModuleManager
{
 public:
	/** Event handler hooks.
	 * This needs to be public to be used by FOREACH_MOD and friends.
	 */
	static std::vector<Module *> EventHandlers[I_END];

	/** Clean up the module runtime directory
	 */
	static void CleanupRuntimeDirectory();

	/** Loads a given module.
	 * @param m the module to load
	 * @param u the user who loaded it, NULL for auto-load
	 * @return MOD_ERR_OK on success, anything else on fail
	 */
	static ModuleReturn LoadModule(const Anope::string &modname, User *u);

	/** Unload the given module.
	 * @param m the module to unload
	 * @param u the user who unloaded it
	 * @return MOD_ERR_OK on success, anything else on fail
	 */
	static ModuleReturn UnloadModule(Module *m, User * u);

	/** Find a module
	 * @param name The module name
	 * @return The module
	 */
	static Module *FindModule(const Anope::string &name);

	/** Find the first module of a certain type
	 * @param type The module type
	 * @return The module
	 */
	static Module *FindFirstOf(ModType type);

	/** Checks whether this version of Anope is at least major.minor.patch.build
	 * Throws a ModuleException if not
	 * @param major The major version
	 * @param minor The minor vesion
	 * @param patch The patch version
	 * @param build The build version
	 */
	static void RequireVersion(int major, int minor, int patch, int build);

	/** Change the priority of one event in a module.
	 * Each module event has a list of modules which are attached to that event type. If you wish to be called before or after other specific modules, you may use this
	 * method (usually within void Module::Prioritize()) to set your events priority. You may use this call in other methods too, however, this is not supported behaviour
	 * for a module.
	 * @param mod The module to change the priority of
	 * @param i The event to change the priority of
	 * @param s The state you wish to use for this event. Use one of
	 * PRIO_FIRST to set the event to be first called, PRIO_LAST to set it to be the last called, or PRIO_BEFORE and PRIO_AFTER
	 * to set it to be before or after one or more other modules.
	 * @param modules If PRIO_BEFORE or PRIO_AFTER is set in parameter 's', then this contains a list of one or more modules your module must be
	 * placed before or after. Your module will be placed before the highest priority module in this list for PRIO_BEFORE, or after the lowest
	 * priority module in this list for PRIO_AFTER.
	 * @param sz The number of modules being passed for PRIO_BEFORE and PRIO_AFTER. Defaults to 1, as most of the time you will only want to prioritize your module
	 * to be before or after one other module.
	 */
	static bool SetPriority(Module *mod, Implementation i, Priority s, Module **modules = NULL, size_t sz = 1);

	/** Change the priority of all events in a module.
	 * @param mod The module to set the priority of
	 * @param s The priority of all events in the module.
	 * Note that with this method, it is not possible to effectively use PRIO_BEFORE or PRIO_AFTER, you should use the more fine tuned
	 * SetPriority method for this, where you may specify other modules to be prioritized against.
	 */
	static bool SetPriority(Module *mod, Priority s);

	/** Attach an event to a module.
	 * You may later detatch the event with ModuleManager::Detach(). If your module is unloaded, all events are automatically detatched.
	 * @param i Event type to attach
	 * @param mod Module to attach event to
	 * @return True if the event was attached
	 */
	static bool Attach(Implementation i, Module *mod);

	/** Detatch an event from a module.
	 * This is not required when your module unloads, as the core will automatically detatch your module from all events it is attached to.
	 * @param i Event type to detach
	 * @param mod Module to detach event from
	 * @param Detach true if the event was detached
	 */
	static bool Detach(Implementation i, Module *mod);

	/** Detach all events from a module (used on unload)
	 * @param mod Module to detach from
	 */
	static void DetachAll(Module *mod);

	/** Attach an array of events to a module
	 * @param i Event types (array) to attach
	 * @param mod Module to attach events to
	 */
	static void Attach(Implementation *i, Module *mod, size_t sz);

	/** Delete all callbacks attached to a module
	 * @param m The module
	 */
	static void ClearCallBacks(Module *m);

	/** Unloading all modules except the protocol module.
	 */
	static void UnloadAll();

 private:
	/** Call the module_delete function to safely delete the module
	 * @param m the module to delete
	 * @return MOD_ERR_OK on success, anything else on fail
	 */
	static ModuleReturn DeleteModule(Module *m);
};

/** Class used for callbacks within modules
 */
class CallBack : public Timer
{
 private:
	Module *m;
 public:
	CallBack(Module *mod, long time_from_now, time_t now = Anope::CurTime, bool repeating = false) : Timer(time_from_now, now, repeating),  m(mod)
	{
		m->CallBacks.push_back(this);
	}

	virtual ~CallBack()
	{
		std::list<CallBack *>::iterator it = std::find(m->CallBacks.begin(), m->CallBacks.end(), this);
		if (it != m->CallBacks.end())
			m->CallBacks.erase(it);
	}
};

template<typename T>
class service_reference : public dynamic_reference<T>
{
	Anope::string type;
	Anope::string name;

 public:
	service_reference(const Anope::string &t, const Anope::string &n) : dynamic_reference<T>(NULL), type(t), name(n)
	{
	}

	inline void operator=(const Anope::string &n)
	{
		this->name = n;
	}

	operator bool()
	{
		if (this->invalid)
		{
			this->invalid = false;
			this->ref = NULL;
		}
		if (!this->ref)
		{
			this->ref = static_cast<T *>(Service::FindService(this->type, this->name));
			if (this->ref)
				this->ref->AddReference(this);
		}
		return this->ref;
	}
};

class CoreExport Message
{
public:
	Anope::string name;
	bool (*func)(const Anope::string &source, const std::vector<Anope::string> &params);

	Message(const Anope::string &n, bool (*f)(const Anope::string &, const std::vector<Anope::string> &));
	~Message();
};

#endif // MODULES_H
