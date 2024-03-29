#ifndef ANOPE_LDAP_H
#define ANOPE_LDAP_H

typedef int LDAPQuery;

class LDAPException : public ModuleException
{
 public:
	LDAPException(const Anope::string &reason) : ModuleException(reason) { }

	virtual ~LDAPException() throw() { }
};

struct LDAPModification
{
	enum LDAPOperation
	{
		LDAP_ADD,
		LDAP_DEL,
		LDAP_REPLACE
	};

	LDAPOperation op;
	Anope::string name;
	std::vector<Anope::string> values;
};
typedef std::vector<LDAPModification> LDAPMods;

struct LDAPAttributes : public std::map<Anope::string, std::vector<Anope::string> >
{
	size_t size(const Anope::string &attr) const
	{
		const std::vector<Anope::string>& array = this->getArray(attr);
		return array.size();
	}

	const std::vector<Anope::string> keys() const
	{
		std::vector<Anope::string> k;
		for (const_iterator it = this->begin(), it_end = this->end(); it != it_end; ++it)
			k.push_back(it->first);
		return k;
	}

	const Anope::string &get(const Anope::string &attr) const
	{
		const std::vector<Anope::string>& array = this->getArray(attr);
		if (array.empty())
			throw LDAPException("Empty attribute " + attr + " in LDAPResult::get");
		return array[0];
	}

	const std::vector<Anope::string>& getArray(const Anope::string &attr) const
	{
		const_iterator it = this->find(attr);
		if (it == this->end())
			throw LDAPException("Unknown attribute " + attr + " in LDAPResult::getArray");
		return it->second;
	}
};

struct LDAPResult
{
	std::vector<LDAPAttributes> messages;
	Anope::string error;

	enum QueryType
	{
		QUERY_BIND,
		QUERY_SEARCH,
		QUERY_ADD,
		QUERY_DELETE,
		QUERY_MODIFY
	};

	QueryType type;
	LDAPQuery id;

	size_t size() const
	{
		return this->messages.size();
	}

	bool empty() const
	{
		return this->messages.empty();
	}

	const LDAPAttributes &get(size_t sz) const
	{
		if (sz >= this->messages.size())
			throw LDAPException("Index out of range");
		return this->messages[sz];
	}

	const Anope::string &getError() const
	{
		return this->error;
	}
};

class LDAPInterface
{
 public:
	Module *owner;

	LDAPInterface(Module *m) : owner(m) { }
	virtual ~LDAPInterface() { }

	virtual void OnResult(const LDAPResult &r) { }

	virtual void OnError(const LDAPResult &err) { }
};

class LDAPProvider : public Service
{
 public:
	LDAPProvider(Module *c, const Anope::string &n) : Service(c, "LDAPProvider", n) { }

	/** Attempt to bind to the LDAP server as an admin
	 * @param i The LDAPInterface the result is sent to
	 * @return The query ID
	 */
	virtual LDAPQuery BindAsAdmin(LDAPInterface *i) = 0;

	/** Bind to LDAP
	 * @param i The LDAPInterface the result is sent to
	 * @param who The binddn
	 * @param pass The password
	 * @return The query ID
	 */
	virtual LDAPQuery Bind(LDAPInterface *i, const Anope::string &who, const Anope::string &pass) = 0;

	/** Search ldap for the specified filter
	 * @param i The LDAPInterface the result is sent to
	 * @param base The base DN to search
	 * @param filter The filter to apply
	 * @return The query ID
	 */
	virtual LDAPQuery Search(LDAPInterface *i, const Anope::string &base, const Anope::string &filter) = 0;

	/** Add an entry to LDAP
	 * @param i The LDAPInterface the result is sent to
	 * @param dn The dn of the entry to add
	 * @param attributes The attributes
	 * @return The query ID
	 */
	virtual LDAPQuery Add(LDAPInterface *i, const Anope::string &dn, LDAPMods &attributes) = 0;

	/** Delete an entry from LDAP
	 * @param i The LDAPInterface the result is sent to
	 * @param dn The dn of the entry to delete
	 * @return The query ID
	 */
	virtual LDAPQuery Del(LDAPInterface *i, const Anope::string &dn) = 0;

	/** Modify an existing entry in LDAP
	 * @param i The LDAPInterface the result is sent to
	 * @param base The base DN to modify
	 * @param attributes The attributes to modify
	 * @return The query ID
	 */
	virtual LDAPQuery Modify(LDAPInterface *i, const Anope::string &base, LDAPMods &attributes) = 0;
};

#endif // ANOPE_LDAP_H
