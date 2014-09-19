/* Modular support
 *
 * (C) 2003-2012 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "modules.h"
#include <algorithm> // std::find

std::vector<Module *> ModuleManager::EventHandlers[I_END];

void ModuleManager::CleanupRuntimeDirectory()
{
	Anope::string dirbuf = services_dir + "/modules/runtime";

	Log(LOG_DEBUG) << "Cleaning out Module run time directory (" << dirbuf << ") - this may take a moment please wait";

	DIR *dirp = opendir(dirbuf.c_str());
	if (!dirp)
	{
		Log(LOG_DEBUG) << "Cannot open directory (" << dirbuf << ")";
		return;
	}
	
	for (dirent *dp; (dp = readdir(dirp));)
	{
		if (!dp->d_ino)
			continue;
		if (Anope::string(dp->d_name).equals_cs(".") || Anope::string(dp->d_name).equals_cs(".."))
			continue;
		Anope::string filebuf = dirbuf + "/" + dp->d_name;
		unlink(filebuf.c_str());
	}

	closedir(dirp);
}

/**
 * Copy the module from the modules folder to the runtime folder.
 * This will prevent module updates while the modules is loaded from
 * triggering a segfault, as the actaul file in use will be in the
 * runtime folder.
 * @param name the name of the module to copy
 * @param output the destination to copy the module to
 * @return MOD_ERR_OK on success
 */
static ModuleReturn moduleCopyFile(const Anope::string &name, Anope::string &output)
{
	Anope::string input = services_dir + "/modules/" + name + ".so";
	
	struct stat s;
	if (stat(input.c_str(), &s) == -1)
		return MOD_ERR_NOEXIST;
	else if (!S_ISREG(s.st_mode))
		return MOD_ERR_NOEXIST;
	
	std::ifstream source(input.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!source.is_open())
		return MOD_ERR_NOEXIST;
	
	char *tmp_output = strdup(output.c_str());
	int target_fd = mkstemp(tmp_output);
	if (target_fd == -1 || close(target_fd) == -1)
	{
		free(tmp_output);
		source.close();
		return MOD_ERR_FILE_IO;
	}
	output = tmp_output;
	free(tmp_output);

	Log(LOG_DEBUG) << "Runtime module location: " << output;
	
	std::ofstream target(output.c_str(), std::ios_base::in | std::ios_base::binary);
	if (!target.is_open())
	{
		source.close();
		return MOD_ERR_FILE_IO;
	}

	int want = s.st_size;
	char *buffer = new char[s.st_size];
	while (want > 0 && !source.fail() && !target.fail())
	{
		source.read(buffer, want);
		int read_len = source.gcount();

		target.write(buffer, read_len);
		want -= read_len;
	}
	delete [] buffer;
	
	source.close();
	target.close();

	return !source.fail() && !target.fail() ? MOD_ERR_OK : MOD_ERR_FILE_IO;
}

/* This code was found online at http://www.linuxjournal.com/article/3687#comment-26593
 *
 * This function will take a pointer from either dlsym or GetProcAddress and cast it in
 * a way that won't cause C++ warnings/errors to come up.
 */
template <class TYPE> TYPE function_cast(void *symbol)
{
	union
	{
		void *symbol;
		TYPE function;
	} cast;
	cast.symbol = symbol;
	return cast.function;
}

ModuleReturn ModuleManager::LoadModule(const Anope::string &modname, User *u)
{
	if (modname.empty())
		return MOD_ERR_PARAMS;

	if (FindModule(modname))
		return MOD_ERR_EXISTS;

	Log(LOG_DEBUG) << "trying to load [" << modname <<  "]";

	/* Generate the filename for the temporary copy of the module */
	Anope::string pbuf = services_dir + "/modules/runtime/" + modname + ".so.XXXXXX";

	/* Don't skip return value checking! -GD */
	ModuleReturn ret = moduleCopyFile(modname, pbuf);
	if (ret != MOD_ERR_OK)
		return ret;

	dlerror();
	void *handle = dlopen(pbuf.c_str(), RTLD_LAZY);
	const char *err = dlerror();
	if (!handle && err && *err)
	{
		Log() << err;
		return MOD_ERR_NOLOAD;
	}

	dlerror();
	Module *(*func)(const Anope::string &, const Anope::string &) = function_cast<Module *(*)(const Anope::string &, const Anope::string &)>(dlsym(handle, "AnopeInit"));
	err = dlerror();
	if (!func && err && *err)
	{
		Log() << "No init function found, not an Anope module";
		dlclose(handle);
		return MOD_ERR_NOLOAD;
	}

	if (!func)
		throw CoreException("Couldn't find constructor, yet moderror wasn't set?");

	/* Create module. */
	Anope::string nick;
	if (u)
		nick = u->nick;

	Module *m;

	try
	{
		m = func(modname, nick);
	}
	catch (const ModuleException &ex)
	{
		Log() << "Error while loading " << modname << ": " << ex.GetReason();
		return MOD_ERR_EXCEPTION;
	}

	m->filename = pbuf;
	m->handle = handle;

	Version v = m->GetVersion();
	if (v.GetMajor() < Anope::VersionMajor() || (v.GetMajor() == Anope::VersionMajor() && v.GetMinor() < Anope::VersionMinor()))
	{
		Log() << "Module " << modname << " is compiled against an older version of Anope " << v.GetMajor() << "." << v.GetMinor() << ", this is " << Anope::VersionMajor() << "." << Anope::VersionMinor();
		DeleteModule(m);
		return MOD_ERR_VERSION;
	}
	else if (v.GetMajor() > Anope::VersionMajor() || (v.GetMajor() == Anope::VersionMajor() && v.GetMinor() > Anope::VersionMinor()))
	{
		Log() << "Module " << modname << " is compiled against a newer version of Anope " << v.GetMajor() << "." << v.GetMinor() << ", this is " << Anope::VersionMajor() << "." << Anope::VersionMinor();
		DeleteModule(m);
		return MOD_ERR_VERSION;
	}
	else if (v.GetBuild() < Anope::VersionBuild())
		Log() << "Module " << modname << " is compiled against an older revision of Anope " << v.GetBuild() << ", this is " << Anope::VersionBuild();
	else if (v.GetBuild() > Anope::VersionBuild())
		Log() << "Module " << modname << " is compiled against a newer revision of Anope " << v.GetBuild() << ", this is " << Anope::VersionBuild();
	else if (v.GetBuild() == Anope::VersionBuild())
		Log(LOG_DEBUG) << "Module " << modname << " compiled against current version of Anope " << v.GetBuild();

	if (m->type == PROTOCOL && ModuleManager::FindFirstOf(PROTOCOL) != m)
	{
		DeleteModule(m);
		Log() << "You cannot load two protocol modules";
		return MOD_ERR_UNKNOWN;
	}

	FOREACH_MOD(I_OnModuleLoad, OnModuleLoad(u, m));

	return MOD_ERR_OK;
}

ModuleReturn ModuleManager::UnloadModule(Module *m, User *u)
{
	if (!m)
		return MOD_ERR_PARAMS;

	FOREACH_MOD(I_OnModuleUnload, OnModuleUnload(u, m));

	return DeleteModule(m);
}

Module *ModuleManager::FindModule(const Anope::string &name)
{
	for (std::list<Module *>::const_iterator it = Modules.begin(), it_end = Modules.end(); it != it_end; ++it)
	{
		Module *m = *it;

		if (m->name.equals_ci(name))
			return m;
	}

	return NULL;
}

Module *ModuleManager::FindFirstOf(ModType type)
{
	for (std::list<Module *>::const_iterator it = Modules.begin(), it_end = Modules.end(); it != it_end; ++it)
	{
		Module *m = *it;

		if (m->type == type)
			return m;
	}

	return NULL;
}

void ModuleManager::RequireVersion(int major, int minor, int patch, int build)
{
	if (Anope::VersionMajor() > major)
		return;
	else if (Anope::VersionMajor() == major)
	{
		if (minor == -1)
			return;
		else if (Anope::VersionMinor() > minor)
			return;
		else if (Anope::VersionMinor() == minor)
		{
			if (patch == -1)
				return;
			else if (Anope::VersionPatch() > patch)
				return;
			else if (Anope::VersionPatch() == patch)
			{
				if (build == -1)
					return;
				else if (Anope::VersionBuild() >= build)
					return;
			}
		}
	}

	throw ModuleException("This module requires version " + stringify(major) + "." + stringify(minor) + "." + stringify(patch) + "-" + build + " - this is " + Anope::Version());
}

ModuleReturn ModuleManager::DeleteModule(Module *m)
{
	if (!m || !m->handle)
		return MOD_ERR_PARAMS;

	void *handle = m->handle;
	Anope::string filename = m->filename;

	Log(LOG_DEBUG) << "Unloading module " << m->name;

	dlerror();
	void (*destroy_func)(Module *m) = function_cast<void (*)(Module *)>(dlsym(m->handle, "AnopeFini"));
	const char *err = dlerror();
	if (!destroy_func || (err && *err))
	{
		Log() << "No destroy function found for " << m->name << ", chancing delete...";
		delete m; /* we just have to chance they haven't overwrote the delete operator then... */
	}
	else
		destroy_func(m); /* Let the module delete it self, just in case */

	if (dlclose(handle))
		Log() << dlerror();

	if (!filename.empty())
		unlink(filename.c_str());
	
	return MOD_ERR_OK;
}

bool ModuleManager::Attach(Implementation i, Module *mod)
{
	if (std::find(EventHandlers[i].begin(), EventHandlers[i].end(), mod) != EventHandlers[i].end())
		return false;

	EventHandlers[i].push_back(mod);
	return true;
}

bool ModuleManager::Detach(Implementation i, Module *mod)
{
	std::vector<Module *>::iterator x = std::find(EventHandlers[i].begin(), EventHandlers[i].end(), mod);

	if (x == EventHandlers[i].end())
		return false;

	EventHandlers[i].erase(x);
	return true;
}

void ModuleManager::Attach(Implementation *i, Module *mod, size_t sz)
{
	for (size_t n = 0; n < sz; ++n)
		Attach(i[n], mod);
}

void ModuleManager::DetachAll(Module *mod)
{
	for (size_t n = I_BEGIN + 1; n != I_END; ++n)
		Detach(static_cast<Implementation>(n), mod);
}

bool ModuleManager::SetPriority(Module *mod, Priority s)
{
	for (size_t n = I_BEGIN + 1; n != I_END; ++n)
		SetPriority(mod, static_cast<Implementation>(n), s);

	return true;
}

bool ModuleManager::SetPriority(Module *mod, Implementation i, Priority s, Module **modules, size_t sz)
{
	/** To change the priority of a module, we first find its position in the vector,
	 * then we find the position of the other modules in the vector that this module
	 * wants to be before/after. We pick off either the first or last of these depending
	 * on which they want, and we make sure our module is *at least* before or after
	 * the first or last of this subset, depending again on the type of priority.
	 */

	/* Locate our module. This is O(n) but it only occurs on module load so we're
	 * not too bothered about it
	 */
	size_t source = 0;
	bool found = false;
	for (size_t x = 0, end = EventHandlers[i].size(); x != end; ++x)
		if (EventHandlers[i][x] == mod)
		{
			source = x;
			found = true;
			break;
		}

	/* Eh? this module doesnt exist, probably trying to set priority on an event
	 * theyre not attached to.
	 */
	if (!found)
		return false;

	size_t swap_pos = 0;
	bool swap = true;
	switch (s)
	{
		/* Dummy value */
		case PRIORITY_DONTCARE:
			swap = false;
			break;
		/* Module wants to be first, sod everything else */
		case PRIORITY_FIRST:
			swap_pos = 0;
			break;
		/* Module is submissive and wants to be last... awww. */
		case PRIORITY_LAST:
			if (EventHandlers[i].empty())
				swap_pos = 0;
			else
				swap_pos = EventHandlers[i].size() - 1;
			break;
		/* Place this module after a set of other modules */
		case PRIORITY_AFTER:
			/* Find the latest possible position */
			swap_pos = 0;
			swap = false;
			for (size_t x = 0, end = EventHandlers[i].size(); x != end; ++x)
				for (size_t n = 0; n < sz; ++n)
					if (modules[n] && EventHandlers[i][x] == modules[n] && x >= swap_pos && source <= swap_pos)
					{
						swap_pos = x;
						swap = true;
					}
			break;
		/* Place this module before a set of other modules */
		case PRIORITY_BEFORE:
			swap_pos = EventHandlers[i].size() - 1;
			swap = false;
			for (size_t x = 0, end = EventHandlers[i].size(); x != end; ++x)
				for (size_t n = 0; n < sz; ++n)
					if (modules[n] && EventHandlers[i][x] == modules[n] && x <= swap_pos && source >= swap_pos)
					{
						swap = true;
						swap_pos = x;
					}
	}

	/* Do we need to swap? */
	if (swap && swap_pos != source)
	{
		/* Suggestion from Phoenix, "shuffle" the modules to better retain call order */
		int incrmnt = 1;

		if (source > swap_pos)
			incrmnt = -1;

		for (unsigned j = source; j != swap_pos; j += incrmnt)
		{
			if (j + incrmnt > EventHandlers[i].size() - 1 || j + incrmnt < 0)
				continue;

			std::swap(EventHandlers[i][j], EventHandlers[i][j + incrmnt]);
		}
	}

	return true;
}

/** Delete all callbacks attached to a module
 * @param m The module
 */
void ModuleManager::ClearCallBacks(Module *m)
{
	while (!m->CallBacks.empty())
		delete m->CallBacks.front();
}

/** Unloading all modules except the protocol module.
 */
void ModuleManager::UnloadAll()
{
	std::vector<Anope::string> modules[MT_END];
	for (std::list<Module *>::iterator it = Modules.begin(), it_end = Modules.end(); it != it_end; ++it)
		if ((*it)->type != PROTOCOL)
			modules[(*it)->type].push_back((*it)->name);

	for (size_t i = MT_BEGIN + 1; i != MT_END; ++i)
		for (unsigned j = 0; j < modules[i].size(); ++j)
		{
			Module *m = FindModule(modules[i][j]);
			if (m != NULL)
				UnloadModule(m, NULL);
		}
}

