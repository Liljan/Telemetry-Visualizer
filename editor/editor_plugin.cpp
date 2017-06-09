#include <editor_plugin_api/editor_plugin_api.h>
#include <plugin_foundation/string.h>

#include <mongoc.h>
#include <bson.h>

#include <string>
#include <vector>

using namespace stingray_plugin_foundation;

namespace PLUGIN_NAMESPACE
{
	ConfigDataApi* config_data_api = nullptr;
	EditorLoggingApi* logging_api = nullptr;
	EditorEvalApi* eval_api = nullptr;

	mongoc_client_t* client = nullptr;
	mongoc_database_t* database = nullptr;
	mongoc_collection_t* collection = nullptr;

	/**
	* Return plugin extension name.
	*/
	const char* get_name() { return "Telemetry Plugin"; }

	/**
	* Return plugin version.
	*/
	const char* get_version() { return "1.0.0"; }

	/**
	* Initialize MongoDB before use.
	*/
	void init_mongoc()
	{
		mongoc_init();
	}

	/**
	* Clean MongoDB before closing.
	*/
	void clean_mongoc()
	{
		if (database != nullptr)
			mongoc_database_destroy(database);

		if (client != nullptr)
			mongoc_client_destroy(client);

		mongoc_cleanup();
	}

	/**
	* This will probably only work with a special database structure.
	* Fetch game sessions associated with a level.
	* Returns an array with session ids.
	*/
	ConfigValue fetch_sessions_ids(ConfigValueArgs args, int num)
	{
		if (num < 1)
			return nullptr;

		auto collection_name = "session_start";

		collection = mongoc_database_get_collection(database, collection_name);

		// Filter by a level key (name of the level)
		auto level_name = config_data_api->to_string(&args[0]);

		mongoc_cursor_t* cursor;
		bson_error_t error;
		const bson_t* doc;
		bson_iter_t iter;
		bson_t opts, filter;
		bson_t project_fields;
		bson_t existspart;

		bson_init(&opts);
		bson_init(&filter);
		bson_init(&existspart);

		BSON_APPEND_INT64(&opts, "limit", 3000); // Limit total amount of documents becasue it becomes too slow otherwise.

		BSON_APPEND_UTF8(&filter, "params.level_key", level_name);

		BSON_APPEND_DOCUMENT_BEGIN(&opts, "projection", &project_fields);
		BSON_APPEND_BOOL(&project_fields, "_id", false);
		BSON_APPEND_BOOL(&project_fields, "session_id", true);
		bson_append_document_end(&opts, &project_fields);

		ConfigValue cv_sessions_ids = config_data_api->make(nullptr);

		cursor = mongoc_collection_find_with_opts(collection, &filter, &opts, NULL);

		while (mongoc_cursor_next(cursor, &doc))
		{
			bson_iter_t field;

			if (!bson_iter_init(&iter, doc) || !bson_iter_find_descendant(&iter, "session_id", &field))
				continue;

			ConfigValue item = config_data_api->make(nullptr);
			auto value = bson_iter_value(&field);

			config_data_api->set_string(item, value->value.v_utf8.str);
			config_data_api->push(cv_sessions_ids, item);

			if (mongoc_cursor_error(cursor, &error))
			{
				fprintf(stderr, "An error occurred: %s\n", error.message);
			}
		}

		mongoc_cursor_destroy(cursor);
		bson_destroy(&opts);
		bson_destroy(&filter);
		bson_destroy(&existspart);

		return cv_sessions_ids;
	}

	/**
	* Fetch documents from the database with the selected filter from the GUI.
	*/
	ConfigValue fetch_documents(ConfigValueArgs args, int num)
	{
		if (num < 1)
			return nullptr;

		auto collection_name = config_data_api->to_string(&args[0]);
		if (collection_name == nullptr)
			return nullptr;

		collection = mongoc_database_get_collection(database, collection_name);

		uint64_t limit, skip;
		std::vector<const char*> filter_fields;
		std::vector<uint8_t> sort_fields;

		// filter by session ids
		bool sessions_ids = false;
		size_t sessions_ind = -1;

		for (auto i = 1; i < num; ++i)
		{
			auto arg = &args[i];
			auto type = config_data_api->type(&args[i]);

			switch (type)
			{
				case CD_TYPE_OBJECT:
				{
					auto object_item_key = config_data_api->object_key(arg, 0);
					auto object_item_value = config_data_api->object_value(arg, 0);
					auto object_item_type = config_data_api->type(object_item_value);

					if (strequal(object_item_key, "limit"))
					{
						if (object_item_type == CD_TYPE_NUMBER)
						{
							limit = config_data_api->to_number(object_item_value);
						}
					}
					else if (strequal(object_item_key, "skip"))
					{
						if (object_item_type == CD_TYPE_NUMBER)
						{
							skip = config_data_api->to_number(object_item_value);
						}
					}
					else if (strequal(object_item_key, "fields"))
					{
						if (object_item_type == CD_TYPE_ARRAY)
						{
							auto length = config_data_api->array_size(object_item_value);
							filter_fields.resize(length);
							for (auto i = 0; i < length; ++i) {
								auto array_item = config_data_api->array_item(object_item_value, i);
								filter_fields[i] = config_data_api->to_string(array_item);
							}
						}
					}
					else if (strequal(object_item_key, "sort"))
					{
						if (object_item_type == CD_TYPE_ARRAY)
						{
							auto length = config_data_api->array_size(object_item_value);
							sort_fields.resize(length);
							for (auto i = 0; i < length; ++i) {
								auto array_item = config_data_api->array_item(object_item_value, i);
								sort_fields[i] = config_data_api->to_bool(array_item);
							}
						}
					}

					// filter by session ids
					else if (strequal(object_item_key, "sessions_ids"))
					{
						sessions_ids = true;
						sessions_ind = i;
						if (object_item_type == CD_TYPE_ARRAY)
						{
							auto length = config_data_api->array_size(object_item_value);
							for (auto i = 0; i < length; ++i) {
								auto array_item = config_data_api->array_item(object_item_value, i);
							}
						}

					}

					break;
				}
				default: break;
			}
		}

		mongoc_cursor_t* cursor = nullptr;
		const bson_t* doc = nullptr;
		bson_iter_t iter;
		bson_t opts, filter;
		bson_t project_fields, sort;
		
		bson_init(&opts);
		bson_init(&filter);

		BSON_APPEND_INT64(&opts, "limit", limit);
		BSON_APPEND_INT64(&opts, "skip", skip);

		// Modified solution for unique database
		if (sessions_ids)
		{
			bson_t or_operand;
			bson_t exist_field_value;
			bson_t session_ids;
			const size_t n_size = 5;
			char char_ind[n_size];

			// Only include documents with a position
			bson_init(&exist_field_value);
			bson_init(&or_operand);
			BSON_APPEND_BOOL(&exist_field_value, "$exists", 1);
			BSON_APPEND_DOCUMENT(&filter, "params.position", &exist_field_value);

			bson_init(&or_operand);

			auto arg = &args[sessions_ind];
			auto object_item_value = config_data_api->object_value(arg, 0);
			auto length = config_data_api->array_size(object_item_value);


			BSON_APPEND_ARRAY_BEGIN(&filter, "$or", &or_operand);

			for (auto i = 0; i < length; ++i)
			{

				sprintf(char_ind, "%d", i);
				auto array_item = config_data_api->array_item(object_item_value, i);
				auto id = config_data_api->to_string(array_item);

				BSON_APPEND_DOCUMENT_BEGIN(&or_operand, char_ind, &session_ids);
				BSON_APPEND_UTF8(&session_ids, "session_id", id);
				bson_append_document_end(&or_operand, &session_ids);
			}

			bson_append_array_end(&filter, &or_operand);

			bson_destroy(&or_operand);
			bson_destroy(&exist_field_value);
		}

		BSON_APPEND_DOCUMENT_BEGIN(&opts, "sort", &sort);
		for (auto i = 0; i < sort_fields.size(); ++i)
		{
			if (sort_fields[i])
				BSON_APPEND_INT32(&sort, filter_fields[i], 1);
		}
		bson_append_document_end(&opts, &sort);

		BSON_APPEND_DOCUMENT_BEGIN(&opts, "projection", &project_fields);
		BSON_APPEND_BOOL(&project_fields, "_id", false); // Ignore _id
		for (auto i = 0; i < filter_fields.size(); ++i)
		{
			BSON_APPEND_BOOL(&project_fields, filter_fields[i], true);
		}
		bson_append_document_end(&opts, &project_fields);

		std::vector<ConfigValue> cv_field_values(filter_fields.size());
		for (auto i = 0; i < filter_fields.size(); ++i)
			cv_field_values[i] = config_data_api->make(nullptr);

		cursor = mongoc_collection_find_with_opts(collection, &filter, &opts, NULL);

		while (mongoc_cursor_next(cursor, &doc))
		{
			for (auto i = 0; i < filter_fields.size(); ++i) {

				bson_iter_t field;

				if (!bson_iter_init(&iter, doc) || !bson_iter_find_descendant(&iter, filter_fields[i], &field))
					continue; // Push nil and continue

				ConfigValue item = config_data_api->make(nullptr);
				auto value = bson_iter_value(&field);

				switch (value->value_type)
				{
					case BSON_TYPE_DOUBLE:
						config_data_api->set_number(item, value->value.v_double);
						config_data_api->push(cv_field_values[i], item);
						break;
					case BSON_TYPE_UTF8:
						config_data_api->set_string(item, value->value.v_utf8.str);
						config_data_api->push(cv_field_values[i], item);
						break;
					case BSON_TYPE_INT32:
						config_data_api->set_number(item, value->value.v_int32);
						config_data_api->push(cv_field_values[i], item);
						break;
					case BSON_TYPE_INT64:
						config_data_api->set_number(item, value->value.v_int64);
						config_data_api->push(cv_field_values[i], item);
						break;
					case BSON_TYPE_BOOL:
						config_data_api->set_bool(item, value->value.v_bool);
						config_data_api->push(cv_field_values[i], item);
						break;
					case BSON_TYPE_DATE_TIME:
						config_data_api->set_number(item, value->value.v_datetime);
						config_data_api->push(cv_field_values[i], item);
						break;
					case BSON_TYPE_TIMESTAMP:
						config_data_api->set_number(item, value->value.v_timestamp.timestamp);
						config_data_api->push(cv_field_values[i], item);
						break;
					case BSON_TYPE_UNDEFINED: case BSON_TYPE_NULL:
						config_data_api->push(cv_field_values[i], config_data_api->nil());
						break;
					default:
						config_data_api->push(cv_field_values[i], config_data_api->nil()); // To do
						break;
				}
			}
		}

		auto cv_documents = config_data_api->make(nullptr);

		for (auto i = 0; i < filter_fields.size(); ++i)
			config_data_api->add_array(cv_documents, filter_fields[i], cv_field_values[i]);

		mongoc_cursor_destroy(cursor);
		bson_destroy(&opts);
		bson_destroy(&filter);

		return cv_documents;
	}

	/**
	* Fetch and return a list of a collections fields keys.
	*/
	ConfigValue fetch_field_keys(ConfigValueArgs args, int num)
	{
		if (num < 1)
			return nullptr;

		const char* collection_name = nullptr;

		std::vector<std::string> field_keys;

		auto arg = &args[0];
		auto cv_field_keys = config_data_api->make(nullptr);
		auto type = config_data_api->type(arg);

		switch (type)
		{
			case CD_TYPE_NULL: return config_data_api->nil();
			case CD_TYPE_STRING:
			{
				collection_name = config_data_api->to_string(arg);
				break;
			}
			default: break;
		}

		if (collection_name == nullptr)
			return nullptr;

		collection = mongoc_database_get_collection(database, collection_name);

		if (collection == nullptr)
			return nullptr;

		mongoc_cursor_t* cursor;
		bson_iter_t iter;
		bson_iter_t sub_iter;
		bson_error_t error;
		const bson_t* doc;
		bson_t opts, filter;
		uint64_t limit = 1;
		bson_t project_field;

		bson_init(&opts);
		bson_init(&filter);
		bson_init(&project_field);

		// Temporary solution for unique database and the collection tech_performance
		if (strequal(collection_name, "tech_performance"))
			BSON_APPEND_INT64(&opts, "skip", 6);

		BSON_APPEND_INT64(&opts, "limit", limit);

		BSON_APPEND_BOOL(&project_field, "_id", false);
		BSON_APPEND_DOCUMENT(&opts, "projection", &project_field);

		auto cv_key = config_data_api->make(nullptr);

		cursor = mongoc_collection_find_with_opts(collection, &filter, &opts, NULL);

		int field_ind = 0;
		while (mongoc_cursor_next(cursor, &doc))
		{

			if (!bson_iter_init(&iter, doc))
				continue;

			while (bson_iter_next(&iter))
			{
				if (!bson_iter_recurse(&iter, &sub_iter))
				{
					auto key = bson_iter_key(&iter);

					config_data_api->set_string(cv_key, key);
					config_data_api->push(cv_field_keys, cv_key);

					field_keys.push_back(key);
				}
				else
				{
					while (bson_iter_next(&sub_iter))
					{
						auto key = bson_iter_key(&iter);
						auto sub_key = bson_iter_key(&sub_iter);

						// Concat the full path
						std::string absolute_path = key;
						absolute_path += ".";
						absolute_path += sub_key;

						field_keys.push_back(absolute_path);

						config_data_api->set_string(cv_key, absolute_path.c_str());
						config_data_api->push(cv_field_keys, cv_key);
					}
				}
			}

			if (mongoc_cursor_error(cursor, &error))
			{
				fprintf(stderr, "An error occurred: %s\n", error.message);
			}
		}

		mongoc_cursor_destroy(cursor);
		bson_destroy(&opts);
		bson_destroy(&filter);
		bson_destroy(&project_field);

		return cv_field_keys;
	}

	/**
	* Fetch and return a list of the database collections.
	*/
	void fetch_collection_names(ConfigValueArgs& cv)
	{
		bson_error_t error;
		char** strv;

		auto temp_cv = config_data_api->make(nullptr);

		if ((strv = mongoc_database_get_collection_names(database, &error)))
		{
			for (auto i = 0; strv[i]; i++)
			{
				config_data_api->set_string(temp_cv, strv[i]);
				config_data_api->push(cv, temp_cv);
			}
			bson_strfreev(strv);
		}
		else
		{
			fprintf(stderr, "Fetch 'collection names' failed: %s\n", error.message);
		}
	}

	/**
	* Connects to a MongoDB server.
	* Return false if it failed.
	*/
	ConfigValue init_server(ConfigValueArgs args, int num)
	{
		if (num < 1)
			return nullptr;

		auto cv_success = config_data_api->make(nullptr);

		auto cv_server = &args[0];
		auto type = config_data_api->type(cv_server);
		switch (type)
		{
			case CD_TYPE_NULL: return config_data_api->nil();
			case CD_TYPE_STRING:
			{
				auto server_adress = config_data_api->to_string(cv_server);
				if (client == nullptr)
				{
					client = mongoc_client_new(server_adress);
					config_data_api->set_bool(cv_success, true);
				}
				else if (!strequal(mongoc_uri_get_string(mongoc_client_get_uri(client)), server_adress)) // Will not nothing if a user is tring to select the already chosen server 
				{
					mongoc_client_destroy(client);
					client = mongoc_client_new(server_adress);
				}
				else
				{
					config_data_api->set_bool(cv_success, false);
				}
				break;
			}
			default: break;
		}

		return cv_success;
	}

	/**
	* Connects to a MongoDB database.
	* Return false if it failed.
	*/
	ConfigValue init_database(ConfigValueArgs args, int num)
	{
		if (num < 1)
			return nullptr;

		auto succsess = true;

		auto cv_database = &args[0];
		auto cv_result = config_data_api->make(nullptr);
		auto type = config_data_api->type(cv_database);
		switch (type)
		{
			case CD_TYPE_NULL: return config_data_api->nil();
			case CD_TYPE_STRING:
			{
				auto database_name = config_data_api->to_string(cv_database);
				if (client == nullptr) // return false If no client is set
				{
					config_data_api->set_bool(cv_result, false);
					succsess = false;
				}
				else if (database == nullptr)
				{
					database = mongoc_client_get_database(client, database_name);
				}
				else if (!strequal(mongoc_database_get_name(database), database_name)) // Will not nothing if a user is tring to select the already chosen database 
				{
					mongoc_database_destroy(database);
					database = mongoc_client_get_database(client, database_name);
				}
				else
				{
					config_data_api->set_bool(cv_result, false);
					succsess = false;
				}

				break;
			}
			default: break;
		}

		if (succsess)
			fetch_collection_names(cv_result); // return an array of the database collection(s)

		return cv_result;
	}

	/**
	* Setup plugin resources and define client JavaScript APIs.
	*/
	void plugin_loaded(GetEditorApiFunction get_editor_api)
	{
		auto api = static_cast<EditorApi*>(get_editor_api(EDITOR_API_ID));
		config_data_api = static_cast<ConfigDataApi*>(get_editor_api(CONFIGDATA_API_ID));
		logging_api = static_cast<EditorLoggingApi*>(get_editor_api(EDITOR_LOGGING_API_ID));
		eval_api = static_cast<EditorEvalApi*>(get_editor_api(EDITOR_EVAL_API_ID));

		init_mongoc();

		api->register_native_function("nativeExtension", "connectToDatabase", &init_server);
		api->register_native_function("nativeExtension", "selectDatabase", &init_database);
		api->register_native_function("nativeExtension", "fetchFieldKeys", &fetch_field_keys);
		api->register_native_function("nativeExtension", "fetchDocuments", &fetch_documents);

		api->register_native_function("nativeExtension", "sessionsIds", &fetch_sessions_ids);
	}

	/**
	* Release plugin resources and exposed APIs.
	*/
	void plugin_unloaded(GetEditorApiFunction get_editor_api)
	{
		auto api = static_cast<EditorApi*>(get_editor_api(EDITOR_API_ID));

		clean_mongoc();

		api->unregister_native_function("nativeExtension", "connectToDatabase");
		api->unregister_native_function("nativeExtension", "selectDatabase");
		api->unregister_native_function("nativeExtension", "fetchFieldNames");
		api->unregister_native_function("nativeExtension", "fetchDocuments");

		api->unregister_native_function("nativeExtension", "sessionsIds");
	}
} // end namespace

  /**
  * Setup plugin APIs.
  */
extern "C" __declspec(dllexport) void* get_editor_plugin_api(unsigned api)
{
	if (api == EDITOR_PLUGIN_SYNC_API_ID)
	{
		static struct EditorPluginSyncApi editor_api = { nullptr };
		editor_api.get_name = &PLUGIN_NAMESPACE::get_name;
		editor_api.get_version = &PLUGIN_NAMESPACE::get_version;
		editor_api.plugin_loaded = &PLUGIN_NAMESPACE::plugin_loaded;
		editor_api.shutdown = &PLUGIN_NAMESPACE::plugin_unloaded;
		return &editor_api;
	}

	return nullptr;
}