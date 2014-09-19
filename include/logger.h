#ifndef LOGGER_H
#define LOGGER_H

enum LogType
{
	LOG_ADMIN,
	LOG_OVERRIDE,
	LOG_COMMAND,
	LOG_SERVER,
	LOG_CHANNEL,
	LOG_USER,
	LOG_NORMAL,
	LOG_TERMINAL,
	LOG_RAWIO,
	LOG_DEBUG,
	LOG_DEBUG_2,
	LOG_DEBUG_3,
	LOG_DEBUG_4
};

struct LogFile
{
	Anope::string filename;

 public:
	std::ofstream stream;

	LogFile(const Anope::string &name);
	Anope::string GetName() const;
};

class Command;

class CoreExport Log
{
 public:
	BotInfo *bi;
	User *u;
	Command *c;
	Channel *chan;
	ChannelInfo *ci;
	Server *s;
	LogType Type;
	Anope::string Category;
	std::list<Anope::string> Sources;

	std::stringstream buf;

	Log(LogType type = LOG_NORMAL, const Anope::string &category = "", BotInfo *bi = NULL);

	/* LOG_COMMAND/OVERRIDE/ADMIN */
	Log(LogType type, User *u, Command *c, ChannelInfo *ci = NULL);

	/* LOG_CHANNEL */
	Log(User *u, Channel *c, const Anope::string &category = "");

	/* LOG_USER */
	explicit Log(User *u, const Anope::string &category = "");

	/* LOG_SERVER */
	Log(Server *s, const Anope::string &category = "");

	Log(BotInfo *b, const Anope::string &category = "");

	~Log();

	Anope::string BuildPrefix() const;

	template<typename T> Log &operator<<(T val)
	{
		this->buf << val;
		return *this;
	}
};

class CoreExport LogInfo
{
 public:
	std::list<Anope::string> Targets;
	std::map<Anope::string, LogFile *> Logfiles;
	std::list<Anope::string> Sources;
	int LogAge;
	std::list<Anope::string> Admin;
	std::list<Anope::string> Override;
	std::list<Anope::string> Commands;
	std::list<Anope::string> Servers;
	std::list<Anope::string> Users;
	std::list<Anope::string> Channels;
	std::list<Anope::string> Normal;
	bool RawIO;
	bool Debug;

	LogInfo(int logage, bool rawio, bool debug);

	~LogInfo();

	void AddType(std::list<Anope::string> &list, const Anope::string &type);

	bool HasType(LogType ltype, const Anope::string &type) const;

	void ProcessMessage(const Log *l);
};

#endif // LOGGER_H

