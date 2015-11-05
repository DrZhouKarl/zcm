#include "TypeDb.hpp"
#include "SymtabElf.hpp"

#define MAXBUFSZ 256
static bool DEBUG = false;

static void *openlib(const string& libname)
{
    // verify the .so library
    size_t len = libname.size();
    if (len < 3 || 0 != strcmp(libname.c_str()+len-3, ".so")) {
        fprintf(stderr, "ERR: bad library name, expected a .so file, not '%s'\n", libname.c_str());
        return NULL;
    }

    // attempt to open the .so
    void *lib = dlopen(libname.c_str(), RTLD_LAZY);
    if (!lib) {
        fprintf(stderr, "ERR: failed to open '%s'\n", libname.c_str());
        fprintf(stderr, "ERR: %s\n", dlerror());
        return NULL;
    }

    return lib;
}

static string methods[] =
{
    "_copy",
    "_decode",
    "_decode_cleanup",
    "_destroy",
    "_encode",
    "_encoded_size",
    "_get_field",
    "_get_type_info",
    "_num_fields",
    "_publish",
    "_struct_size",
    "_subscribe",
    //"_subscription_set_queue_capacity",
    "_unsubscribe"
};

static void printMissingMethods(uint32_t mask)
{
    printf("  Missing methods:\n");

    int count = sizeof(methods)/sizeof(methods[0]);
    for(int i = 0; i < count; i++) {
        if(!(mask&(1<<i))) {
            printf("    %s\n", methods[i].c_str());
        }
    }
}

// find all zcm types by post-processing the symbols
static bool findTypenames(vector<string>& result, const string& libname)
{
    // read the library's symbol table
    SymtabElf stbl{libname};
    if (!stbl.good()) {
        fprintf(stderr, "ERR: failed to load symbol table for ELF file\n");
        return false;
    }

    const size_t nmethods = sizeof(methods)/sizeof(methods[0]);

    // name -> mask
    unordered_map<string,uint32_t> names;

    // process the symbols
    string s;
    while(stbl.getNext(s)) {
        for(size_t i = 0; i < nmethods; i++) {
            auto& m = methods[i];
            if (!StringUtil::endswith(s, m))
                continue;

            // construct the typename
            string name = s.substr(0, s.size()-m.size());
            if(DEBUG) printf("found potential typename='%s'\n", name.c_str());

            // get or create mask
            uint32_t& mask = names[name];

            // set this mask
            mask |= 1<<i;
        }
    }

    // prune the names array using the masks
    uint32_t validmask = (1 << nmethods) - 1;
    for (auto& it : names) {
        auto& nm = it.first;
        uint32_t mask = it.second;
        if (mask == validmask) {
            if(DEBUG) printf("verified new zcmtype: %s\n", nm.c_str());
            result.push_back(nm);
        } else {
            if(DEBUG) printf("rejecting type '%s' with mask 0x%x\n", nm.c_str(), mask);
            if(DEBUG) printMissingMethods(mask);
        }
    }

    return true;
}

bool TypeDb::loadtypes(const string& libname, void *lib)
{
    vector<string> names;
    if (!findTypenames(names, libname)) {
        fprintf(stderr, "ERR: failed to find zcm typenames in %s\n", libname.c_str());
        return false;
    }

    // fetch each zcm_type_info_t*, compute each hash, and add to hashtable
    for (auto& nm : names) {
        if(DEBUG) printf("Attempting load for type %s\n", nm.c_str());

        string funcname = nm + "_get_type_info";
        zcm_type_info_t *(*get_type_info)(void) = NULL;
        *(void **) &get_type_info = dlsym(lib, funcname.c_str());
        if(get_type_info == NULL) {
            fprintf(stderr, "ERR: failed to load %s\n", funcname.c_str());
            continue;
        }

        zcm_type_info_t *typeinfo = get_type_info();
        TypeMetadata md;
        md.hash = typeinfo->get_hash();
        md.name = nm;
        md.info = typeinfo;

        hashToType[md.hash] = md;
        nameToHash[md.name] = md.hash;

        if(DEBUG) printf("Success loading type %s (0x%" PRIx64 ")\n", nm.c_str(), md.hash);
    }

    if(DEBUG) printf("Loaded %d zcmtypes from %s\n", (int)hashToType.size(), libname.c_str());

    return true;
}

TypeDb::TypeDb(const string& paths, bool debug)
: debug(debug)
{
    DEBUG = debug;
    for (auto& libname : StringUtil::split(paths, ':')) {
        if(DEBUG) printf("Loading types from '%s'\n", libname.c_str());
        void *lib = openlib(libname);
        if (lib == NULL) {
            fprintf(stderr, "Err: failed to open '%s'\n", libname.c_str());
            continue;
        }
        if (!loadtypes(libname, lib)) {
            fprintf(stderr, "Err: failed to load types from '%s'\n", libname.c_str());
            continue;
        }
    }
}

const TypeMetadata *TypeDb::getByHash(i64 hash)
{
    return lookup(hashToType, hash);
}

const TypeMetadata *TypeDb::getByName(const string& name)
{
    if (i64 *hash = lookup(nameToHash, name)) {
        return getByHash(*hash);
    } else {
        return NULL;
    }
}
