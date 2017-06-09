#include <engine_plugin_api/plugin_api.h>
#include <plugin_foundation/platform.h>
#include <plugin_foundation/id_string.h>
#include <plugin_foundation/string.h>
#include <plugin_foundation/allocator.h>

#include <mongoc.h>
#include <bson.h>
#include <bcon.h>

#include <string.h>
#include <string>

#if _DEBUG
#include <stdlib.h>
#include <time.h>
#endif

namespace PLUGIN_NAMESPACE
{
	using namespace stingray_plugin_foundation;

	// Common constants
	unsigned int INVALID_HANDLE = UINT32_MAX;

	// Engine APIs
	DataCompilerApi* data_compiler = nullptr;
	DataCompileParametersApi* data_compile_params = nullptr;
	AllocatorApi* allocator_api = nullptr;
	AllocatorObject* allocator_object = nullptr;
	ApiAllocator _allocator(nullptr, nullptr);
	LoggingApi* log = nullptr;
	ErrorApi* error = nullptr;
	UnitApi* unit = nullptr;
	ResourceManagerApi* resource_manager = nullptr;
	RenderBufferApi* render_buffer = nullptr;
	LuaApi* lua = nullptr;

	mongoc_client_t* client = nullptr;
	mongoc_database_t* database = nullptr;
	mongoc_collection_t* collection = nullptr;

	constexpr int MAX_FIELDS = 10;

	struct Field
	{
		const char* name = nullptr;
		uint8_t ind = -1;
	};

	// C Scripting API
	namespace stingray
	{
		struct UnitCApi* Unit = nullptr;
		struct MeshCApi* Mesh = nullptr;
		struct MaterialCApi* Material = nullptr;
		struct DynamicScriptDataCApi* Data = nullptr;
	}

	// Data compiler resource properties
	int RESOURCE_VERSION = 1;
	const char* RESOURCE_EXTENSION = "my_file_extension";
	const IdString64 RESOURCE_ID = IdString64(RESOURCE_EXTENSION);

	/**
	* Returns the plugin name.
	*/
	const char* get_name() { return "Telemetry_plugin"; }

	void init_mongoc()
	{
		mongoc_init();
	}

	void clean_mongoc()
	{
		if (database != nullptr)
			mongoc_database_destroy(database);

		if (client != nullptr)
			mongoc_client_destroy(client);

		mongoc_cleanup();
	}

	int connect_to_database(struct lua_State *L)
	{
		auto database_adress = lua->tolstring(L, 1, nullptr);

		if (database_adress == nullptr)
			return 0;

		if (client == nullptr)
		{
			client = mongoc_client_new(database_adress);

			lua->pushboolean(L, true);
		}
		else if (!strequal(mongoc_uri_get_string(mongoc_client_get_uri(client)), database_adress))
		{
			mongoc_client_destroy(client);

			client = mongoc_client_new(database_adress);

			lua->pushboolean(L, true);
		}
		else
		{
			lua->pushboolean(L, false);
		}

		return 1;
	}

	int select_database(struct lua_State *L)
	{
		auto database_name = lua->tolstring(L, 1, nullptr);

		if (database_name == nullptr)
			return 0;

		if (client == nullptr)
		{
			lua->pushboolean(L, false);
		}
		else if (database == nullptr)
		{
			database = mongoc_client_get_database(client, database_name);

			lua->pushboolean(L, true);
		}
		else if (!strequal(mongoc_database_get_name(database), database_name))
		{
			mongoc_database_destroy(database);
			database = mongoc_client_get_database(client, database_name);

			lua->pushboolean(L, true);
		}
		else
		{
			lua->pushboolean(L, false);
		}

		return 1;
	}

	// TODO remove hardcode text
	void filter_database_fetch(struct lua_State *L, bson_t *filter)
	{
		bson_t session_ids;
		bson_t existspart;
		bson_t or_operand;
		size_t ind = 0;
		const size_t n_size = 5;
		char char_ind[n_size];

		// Only include documents with a position
		bson_init(&existspart);
		BSON_APPEND_BOOL(&existspart, "$exists", 1);
		BSON_APPEND_DOCUMENT(filter, "params.position", &existspart);

		lua->pushnil(L);
		bson_init(&or_operand);

		BSON_APPEND_ARRAY_BEGIN(filter, "$or", &or_operand);

		while (lua->next(L, 4))
		{

			sprintf(char_ind, "%d", ind);
			auto arr = lua->tolstring(L, -1, nullptr);

			BSON_APPEND_DOCUMENT_BEGIN(&or_operand, char_ind, &session_ids);
			BSON_APPEND_UTF8(&session_ids, "session_id", arr);
			bson_append_document_end(&or_operand, &session_ids);

			lua->pop(L);

			++ind;
		}

		bson_append_array_end(filter, &or_operand);

		bson_destroy(&or_operand);
		bson_destroy(&existspart);
	}

	void filter_project_field(Field *fields, bson_t *opts)
	{
		bson_t project_field;

		BSON_APPEND_DOCUMENT_BEGIN(opts, "projection", &project_field);

		BSON_APPEND_BOOL(&project_field, "_id", false); // Ignore _id

		size_t ind = 0;
		while (fields[ind].name != nullptr)
		{
			BSON_APPEND_BOOL(&project_field, fields[ind].name, true);
			++ind;
		}

		bson_append_document_end(opts, &project_field);
	}

	// TODO sort by user choice
	void sort_data_by(bson_t *opts)
	{
		bson_t sort;

		BSON_APPEND_DOCUMENT_BEGIN(opts, "sort", &sort);
		BSON_APPEND_INT32(&sort, "tick", 1);
		bson_append_document_end(opts, &sort);
	}

	void create_lua_tables(struct lua_State *L, Field *fields, bson_t *filter, bson_t *opts)
	{
		bson_error_t error;

		auto n_objects = mongoc_collection_count_with_opts(collection, MONGOC_QUERY_NONE, filter, 0, 0, opts, NULL, &error);

		size_t ind = 0;
		while (fields[ind].name != nullptr)
		{
			if (!strcmp(fields[ind].name, "params.position"))
				lua->createtable(L, n_objects, 0);
			else
				lua->createtable(L, 3 * n_objects, 0);

			fields[ind].ind = lua->gettop(L);
			++ind;
		}
	}

	int fetch_field_data(struct lua_State *L)
	{
		auto coll = lua->tolstring(L, 1, nullptr);

		if (coll == nullptr)
			return 0;

		collection = mongoc_database_get_collection(database, coll);

		if (collection == nullptr)
			return 0;

		Field field_table[MAX_FIELDS];
		int n_fields = 0;

		lua->pushnil(L);
		while (lua->next(L, 2))
		{
			auto field_name = lua->tolstring(L, -1, nullptr);

			field_table[n_fields].name = field_name;
			++n_fields;

			lua->pop(L);
		}

		uint64_t limit = lua->tonumber(L, 3);

		mongoc_cursor_t* cursor;
		bson_error_t error;
		const bson_t* doc;
		bson_iter_t iter;
		bson_t opts, filter;

		bson_init(&opts);
		bson_init(&filter);

		BSON_APPEND_INT64(&opts, "limit", limit);
		filter_database_fetch(L, &filter);
		filter_project_field(field_table, &opts);
		sort_data_by(&opts);
		create_lua_tables(L, field_table, &filter, &opts);

		auto obj_n = 1;
		cursor = mongoc_collection_find_with_opts(collection, &filter, &opts, NULL);
		log->info(get_name(), "Fetch data from the database and process");

		while (mongoc_cursor_next(cursor, &doc))
		{

			for (auto f = 0; f < sizeof(field_table); ++f) {

				bson_iter_t field;

				if (!bson_iter_init(&iter, doc) || !bson_iter_find_descendant(&iter, field_table[f].name, &field))
					continue; // Push nil and continue

				auto key = bson_iter_key(&field);
				auto value = bson_iter_value(&field);

				if (strcmp(key, "position"))
				{
					switch (value->value_type)
					{
					case BSON_TYPE_UTF8:
						lua->pushstring(L, value->value.v_utf8.str);
						lua->rawseti(L, field_table[f].ind, obj_n);
						break;
					case BSON_TYPE_DOUBLE:
						lua->pushnumber(L, value->value.v_double);
						lua->rawseti(L, field_table[f].ind, obj_n);
						break;
					case BSON_TYPE_INT32:
						lua->pushnumber(L, value->value.v_int32);
						lua->rawseti(L, field_table[f].ind, obj_n);
						break;
					case BSON_TYPE_BOOL:
						lua->pushboolean(L, value->value.v_bool);
						lua->rawseti(L, field_table[f].ind, obj_n);
						break;
					case BSON_TYPE_UNDEFINED:
						lua->pushnil(L);
						lua->rawseti(L, field_table[f].ind, obj_n);
					default:
						lua->pushnil(L);
						lua->rawseti(L, field_table[f].ind, obj_n);
						break;
					}
				}
				else
				{
					double x, y, z;
					auto* p = value->value.v_utf8.str + strlen32("Vector3(");

					x = atof(p);

					for (; p[0] != ','; ++p) {}
					++p;
					y = atof(p);

					for (; p[0] != ','; ++p) {}
					++p;
					z = atof(p);

					lua->pushnumber(L, x);
					lua->rawseti(L, field_table[f].ind, obj_n * 3 - 2);

					lua->pushnumber(L, y);
					lua->rawseti(L, field_table[f].ind, obj_n * 3 + 1 - 2);

					lua->pushnumber(L, z);
					lua->rawseti(L, field_table[f].ind, obj_n * 3 + 2 - 2);
				}
			}
			++obj_n;

			if (mongoc_cursor_error(cursor, &error))
			{
				fprintf(stderr, "An error occurred: %s\n", error.message);
			}
		}

		log->info(get_name(), "The data is processed");

		mongoc_cursor_destroy(cursor);

		bson_destroy(&filter);
		bson_destroy(&opts);

		return n_fields;
	}

	/**
	* Clone the source resource data and append it's buffer size in front of the
	* data result buffer.
	*/
	DataCompileResult pack_source_data_with_size(DataCompileParameters* input, const DataCompileResult& result_data)
	{
		DataCompileResult result = { nullptr };
		unsigned length_with_data_size = result_data.data.len + sizeof(unsigned);
		result.data.p = (char*)allocator_api->allocate(data_compile_params->allocator(input), length_with_data_size, 4);
		result.data.len = length_with_data_size;
		memcpy(result.data.p, &result_data.data.len, sizeof(unsigned));
		memcpy(result.data.p + sizeof(unsigned), result_data.data.p, result_data.data.len);
		return result;
	}

	/**
	* Define plugin resource compiler.
	*/
	DataCompileResult my_resource_compiler(DataCompileParameters* input)
	{
		auto source_data = data_compile_params->read(input);
		if (source_data.error)
			return source_data;
		return pack_source_data_with_size(input, source_data);
	}

	/**
	* Setup runtime and compiler common resources, such as allocators.
	*/
	void setup_common_api(GetApiFunction get_engine_api)
	{
		allocator_api = (AllocatorApi*)get_engine_api(ALLOCATOR_API_ID);
		if (allocator_object == nullptr)
		{
			allocator_object = allocator_api->make_plugin_allocator(get_name());
			_allocator = ApiAllocator(allocator_api, allocator_object);
		}

		log = (LoggingApi*)get_engine_api(LOGGING_API_ID);
		error = (ErrorApi*)get_engine_api(ERROR_API_ID);
		resource_manager = (ResourceManagerApi*)get_engine_api(RESOURCE_MANAGER_API_ID);
	}

	/**
	* Setup plugin runtime resources.
	*/
	void setup_plugin(GetApiFunction get_engine_api)
	{
		setup_common_api(get_engine_api);

		init_mongoc();

		unit = (UnitApi*)get_engine_api(UNIT_API_ID);
		render_buffer = (RenderBufferApi*)get_engine_api(RENDER_BUFFER_API_ID);
		auto c_api = (ScriptApi*)get_engine_api(C_API_ID);
		stingray::Unit = c_api->Unit;
		stingray::Mesh = c_api->Mesh;
		stingray::Material = c_api->Material;
		stingray::Data = c_api->DynamicScriptData;

		lua = (LuaApi*)get_engine_api(LUA_API_ID);

		lua->add_module_function(get_name(), "connect_to_database", connect_to_database);
		lua->add_module_function(get_name(), "select_database", select_database);
		lua->add_module_function(get_name(), "fetch_field_data", fetch_field_data);
	}

	/**
	* Setup plugin compiler resources.
	*/
	void setup_data_compiler(GetApiFunction get_engine_api)
	{
		setup_common_api(get_engine_api);

#if _DEBUG
		// Always trigger the resource compiler in debug mode.
		srand(time(nullptr));
		RESOURCE_VERSION = rand();
#endif

		data_compiler = (DataCompilerApi*)get_engine_api(DATA_COMPILER_API_ID);
		data_compile_params = (DataCompileParametersApi*)get_engine_api(DATA_COMPILE_PARAMETERS_API_ID);
		data_compiler->add_compiler(RESOURCE_EXTENSION, RESOURCE_VERSION, my_resource_compiler);
	}

	/**
	* Indicate to the resource manager that we'll be using our plugin resource type.
	*/
	void setup_resources(GetApiFunction get_engine_api)
	{
		setup_common_api(get_engine_api);
		resource_manager->register_type(RESOURCE_EXTENSION);
	}

	/**
	* Called per game frame.
	*/
	void update_plugin(float dt)
	{
		//log->info(get_name(), error->eprintf("Updating %f", dt));
	}

	/**
	* Release plugin resources.
	*/
	void shutdown_plugin()
	{
		if (allocator_object != nullptr)
		{
			XENSURE(_allocator.api());
			_allocator = ApiAllocator(nullptr, nullptr);
			allocator_api->destroy_plugin_allocator(allocator_object);
			allocator_object = nullptr;

			clean_mongoc();
			lua->remove_all_module_entries(get_name());
		}


	}
}

extern "C"
{
	/**
	* Load and define plugin APIs.
	*/
	PLUGIN_DLLEXPORT void* get_plugin_api(unsigned api)
	{
		using namespace PLUGIN_NAMESPACE;

		if (api == PLUGIN_API_ID)
		{
			static PluginApi plugin_api = { nullptr };
			plugin_api.get_name = get_name;
			plugin_api.setup_game = setup_plugin;
			plugin_api.update_game = update_plugin;
			plugin_api.setup_resources = setup_resources;
			plugin_api.shutdown_game = shutdown_plugin;
			//plugin_api.setup_data_compiler = setup_data_compiler;
			//plugin_api.shutdown_data_compiler = shutdown_plugin;

			return &plugin_api;
		}
		return nullptr;
	}
}
