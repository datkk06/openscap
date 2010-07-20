/**
 * @file oval_defModel.c
 * \brief Open Vulnerability and Assessment Language
 *
 * See more details at http://oval.mitre.org/
 */

/*
 * Copyright 2009-2010 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "David Niemoller" <David.Niemoller@g2-inc.com>
 */

#include <string.h>
#include <time.h>
#include "config.h"
#include "oval_definitions_impl.h"
#include "oval_agent_api_impl.h"
#include "oval_parser_impl.h"
#include "oval_string_map_impl.h"
#include "oval_system_characteristics_impl.h"
#include "oval_probe_impl.h"
#include "oval_results_impl.h"
#include "common/util.h"
#include "common/debug_priv.h"
#include "common/_error.h"
#include "common/reporter_priv.h"

typedef struct oval_definition_model {
	struct oval_string_map *definition_map;
	struct oval_string_map *test_map;
	struct oval_string_map *object_map;
	struct oval_string_map *state_map;
	struct oval_string_map *variable_map;
	bool is_locked;
        char *schema;
} oval_definition_model_t;

/* TODO: Use this to generate links
static char * _oval_generate_schema_location(const char * version)
{

        char * schema = oscap_alloc(sizeof(char)*(strlen(OVAL_DEF_SCHEMA_LOCATION_DEF_PX)+strlen(version)+strlen(OVAL_DEF_SCHEMA_LOCATION_DEF_SX)+2));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_DEF_PX, strlen(OVAL_DEF_SCHEMA_LOCATION_DEF_PX));
        strncat(schema, version, strlen(version));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_DEF_SX, strlen(OVAL_DEF_SCHEMA_LOCATION_DEF_SX));
        strncat(schema, " ", 1);

        oscap_realloc(schema, sizeof(char)*(strlen(schema)+strlen(OVAL_DEF_SCHEMA_LOCATION_IND_PX)+strlen(version)+strlen(OVAL_DEF_SCHEMA_LOCATION_IND_SX)+1));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_IND_PX, strlen(OVAL_DEF_SCHEMA_LOCATION_IND_PX));
        strncat(schema, version, strlen(version));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_IND_SX, strlen(OVAL_DEF_SCHEMA_LOCATION_IND_SX));
        strncat(schema, " ", 1);

        oscap_realloc(schema, sizeof(char)*(strlen(schema)+strlen(OVAL_DEF_SCHEMA_LOCATION_UNX_PX)+strlen(version)+strlen(OVAL_DEF_SCHEMA_LOCATION_UNX_SX)+1));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_UNX_PX, strlen(OVAL_DEF_SCHEMA_LOCATION_UNX_PX));
        strncat(schema, version, strlen(version));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_UNX_SX, strlen(OVAL_DEF_SCHEMA_LOCATION_UNX_SX));
        strncat(schema, " ", 1);

        oscap_realloc(schema, sizeof(char)*(strlen(schema)+strlen(OVAL_DEF_SCHEMA_LOCATION_LNX_PX)+strlen(version)+strlen(OVAL_DEF_SCHEMA_LOCATION_LNX_SX)+1));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_LNX_PX, strlen(OVAL_DEF_SCHEMA_LOCATION_LNX_PX));
        strncat(schema, version, strlen(version));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_LNX_SX, strlen(OVAL_DEF_SCHEMA_LOCATION_LNX_SX));
        strncat(schema, " ", 1);

        oscap_realloc(schema, sizeof(char)*(strlen(schema)+strlen(OVAL_DEF_SCHEMA_LOCATION_CMN_PX)+strlen(version)+strlen(OVAL_DEF_SCHEMA_LOCATION_CMN_SX)+1));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_CMN_PX, strlen(OVAL_DEF_SCHEMA_LOCATION_CMN_PX));
        strncat(schema, version, strlen(version));
        strncat(schema, OVAL_DEF_SCHEMA_LOCATION_CMN_SX, strlen(OVAL_DEF_SCHEMA_LOCATION_CMN_SX));


        return schema;
}*/

/* failed   - NULL
 * success  - oval_definition_model
 * */
struct oval_definition_model *oval_definition_model_new()
{
	oval_definition_model_t *newmodel = (oval_definition_model_t *) oscap_alloc(sizeof(oval_definition_model_t));
	if (newmodel == NULL)
		return NULL;

	newmodel->definition_map = oval_string_map_new();
	newmodel->object_map = oval_string_map_new();
	newmodel->state_map = oval_string_map_new();
	newmodel->test_map = oval_string_map_new();
	newmodel->variable_map = oval_string_map_new();
	newmodel->is_locked = false;
        newmodel->schema = strdup(OVAL_DEF_SCHEMA_LOCATION);
	return newmodel;
}

void oval_definition_model_lock(struct oval_definition_model *definition_model)
{
	__attribute__nonnull__(definition_model);

	if (definition_model && oval_definition_model_is_valid(definition_model))
		definition_model->is_locked = true;
}

bool oval_definition_model_is_locked(struct oval_definition_model *definition_model)
{
	__attribute__nonnull__(definition_model);

	return definition_model->is_locked;
}

bool oval_definition_model_is_valid(struct oval_definition_model * definition_model)
{
	bool is_valid = true;
	struct oval_definition_iterator *definitions_itr;
	struct oval_test_iterator *tests_itr;
	struct oval_object_iterator *objects_itr;
	struct oval_state_iterator *states_itr;

	if (definition_model == NULL) {
                oscap_dprintf("WARNING: argument is not valid: NULL.\n");
		return false;
        }

	/* validate definitions */
	definitions_itr = oval_definition_model_get_definitions(definition_model);
	while (oval_definition_iterator_has_more(definitions_itr)) {
		struct oval_definition *definition;

		definition = oval_definition_iterator_next(definitions_itr);
		if (oval_definition_is_valid(definition) != true) {
			is_valid = false;
			break;
		}
	}
	oval_definition_iterator_free(definitions_itr);
	if (is_valid != true)
		return false;

	/* validate tests */
	tests_itr = oval_definition_model_get_tests(definition_model);
	while (oval_test_iterator_has_more(tests_itr)) {
		struct oval_test *test;

		test = oval_test_iterator_next(tests_itr);
		if (oval_test_is_valid(test) != true) {
			is_valid = false;
			break;
		}
	}
	oval_test_iterator_free(tests_itr);
	if (is_valid != true)
		return false;

	/* validate objects */
	objects_itr = oval_definition_model_get_objects(definition_model);
	while (oval_object_iterator_has_more(objects_itr)) {
		struct oval_object *obj;

		obj = oval_object_iterator_next(objects_itr);
		if (oval_object_is_valid(obj) != true) {
			is_valid = false;
			break;
		}
	}
	oval_object_iterator_free(objects_itr);
	if (is_valid != true)
		return false;

	/* validate states */
	states_itr = oval_definition_model_get_states(definition_model);
	while (oval_state_iterator_has_more(states_itr)) {
		struct oval_state *state;
		state = oval_state_iterator_next(states_itr);
		if (oval_state_is_valid(state) != true) {
			is_valid = false;
			break;
		}
	}
	oval_state_iterator_free(states_itr);
	if (is_valid != true)
		return false;

	return true;
}

typedef void (*_oval_clone_func) (void *, struct oval_definition_model *);

static void _oval_definition_model_clone(struct oval_string_map *oldmap, 
					 struct oval_definition_model *newmodel, 
					 _oval_clone_func cloner)
{
	struct oval_string_iterator *keys = (struct oval_string_iterator *)oval_string_map_keys(oldmap);
	while (oval_string_iterator_has_more(keys)) {
		char *key = oval_string_iterator_next(keys);
		void *olditem = oval_string_map_get_value(oldmap, key);
		(*cloner) (newmodel, olditem);
	}
	oval_string_iterator_free(keys);
}

/* failed   - NULL
 * success  - oval_definition_model
 * */
struct oval_definition_model *oval_definition_model_clone(struct oval_definition_model *oldmodel)
{
	__attribute__nonnull__(oldmodel);

	struct oval_definition_model *newmodel = oval_definition_model_new();
	if (newmodel == NULL)
		return NULL;

	_oval_definition_model_clone
	    (oldmodel->definition_map, newmodel, (_oval_clone_func) oval_definition_clone);
	_oval_definition_model_clone
	    (oldmodel->object_map, newmodel, (_oval_clone_func) oval_object_clone);
	_oval_definition_model_clone(oldmodel->state_map, newmodel, (_oval_clone_func) oval_state_clone);
	_oval_definition_model_clone(oldmodel->test_map, newmodel, (_oval_clone_func) oval_test_clone);
	_oval_definition_model_clone
	    (oldmodel->variable_map, newmodel, (_oval_clone_func) oval_variable_clone);
        newmodel->schema = oscap_strdup(oldmodel->schema);
	return newmodel;
}

void oval_definition_model_free(struct oval_definition_model *model)
{
	__attribute__nonnull__(model);

	oval_string_map_free(model->definition_map, (oscap_destruct_func) oval_definition_free);
	oval_string_map_free(model->object_map, (oscap_destruct_func) oval_object_free);
	oval_string_map_free(model->state_map, (oscap_destruct_func) oval_state_free);
	oval_string_map_free(model->test_map, (oscap_destruct_func) oval_test_free);
	oval_string_map_free(model->variable_map, (oscap_destruct_func) oval_variable_free);

        if (model->schema != NULL)
            oscap_free(model->schema);

	model->definition_map = NULL;
	model->object_map = NULL;
	model->state_map = NULL;
	model->test_map = NULL;
	model->variable_map = NULL;
        model->schema = NULL;

	oscap_free(model);
}

const char * oval_definition_model_get_schema(struct oval_definition_model * model)
{
        __attribute__nonnull__(model);

        return model->schema;
}

void oval_definition_model_add_definition(struct oval_definition_model *model, struct oval_definition *definition)
{
	if (model && !oval_definition_model_is_locked(model)) {
		char *key = oval_definition_get_id(definition);
		oval_string_map_put(model->definition_map, key, (void *)definition);
	} else
		oscap_dprintf("WARNING: attempt to update locked content (%s:%d)", __FILE__, __LINE__);
}

void oval_definition_model_set_schema(struct oval_definition_model *model, const char *version)
{
	if (model && !oval_definition_model_is_locked(model)) {
                model->schema = oscap_strdup(version);
	} else
		oscap_dprintf("WARNING: attempt to update locked content (%s:%d)", __FILE__, __LINE__);
}

void oval_definition_model_add_test(struct oval_definition_model *model, struct oval_test *test)
{
	if (model && !oval_definition_model_is_locked(model)) {
		char *key = oval_test_get_id(test);
		oval_string_map_put(model->test_map, key, (void *)test);
	} else
		oscap_dprintf("WARNING: attempt to update locked content (%s:%d)", __FILE__, __LINE__);
}

void oval_definition_model_add_object(struct oval_definition_model *model, struct oval_object *object)
{
	if (model && !oval_definition_model_is_locked(model)) {
		char *key = oval_object_get_id(object);
		oval_string_map_put(model->object_map, key, (void *)object);
	} else
		oscap_dprintf("WARNING: attempt to update locked content (%s:%d)", __FILE__, __LINE__);
}

void oval_definition_model_add_state(struct oval_definition_model *model, struct oval_state *state)
{
	if (model && !oval_definition_model_is_locked(model)) {
		char *key = oval_state_get_id(state);
		oval_string_map_put(model->state_map, key, (void *)state);
	} else
		oscap_dprintf("WARNING: attempt to update locked content (%s:%d)", __FILE__, __LINE__);
}

void oval_definition_model_add_variable(struct oval_definition_model *model, struct oval_variable *variable)
{
	if (model && !oval_definition_model_is_locked(model)) {
		char *key = oval_variable_get_id(variable);
		oval_string_map_put(model->variable_map, key, (void *)variable);
	} else
		oscap_dprintf("WARNING: attempt to update locked content (%s:%d)", __FILE__, __LINE__);
}

struct oval_definition_model * oval_definition_model_import(const char *file)
{
        struct oval_definition_model *model = oval_definition_model_new();
        int ret = oval_definition_model_merge(model,file);
        if (ret == -1) {
                oval_definition_model_free(model);
                model = NULL;
        }

        return model;
}

int oval_definition_model_merge(struct oval_definition_model *model, const char *file)
{
	__attribute__nonnull__(model);

	xmlTextReader *reader = xmlNewTextReaderFilename(file);

	int retcode = 0;
	if (reader) {
		if (xmlTextReaderRead(reader) > -1) {
			retcode = ovaldef_parser_parse(model, reader, NULL);
		}
		xmlFreeTextReader(reader);
	} else {
		oscap_setxmlerr(xmlGetLastError());
		return -1;
	}

	return retcode;
}

struct oval_definition *oval_definition_model_get_definition(struct oval_definition_model *model, const char *key)
{
	__attribute__nonnull__(model);

	return (struct oval_definition *)oval_string_map_get_value(model->definition_map, key);
}

struct oval_test *oval_definition_model_get_test(struct oval_definition_model *model, const char *key)
{
	__attribute__nonnull__(model);

	return (struct oval_test *)oval_string_map_get_value(model->test_map, key);
}

struct oval_object *oval_definition_model_get_object(struct oval_definition_model *model, const char *key)
{
	__attribute__nonnull__(model);

	return (struct oval_object *)oval_string_map_get_value(model->object_map, key);
}

struct oval_state *oval_definition_model_get_state(struct oval_definition_model *model, const char *key)
{
	__attribute__nonnull__(model);

	return (struct oval_state *)oval_string_map_get_value(model->state_map, key);
}

struct oval_variable *oval_definition_model_get_variable(struct oval_definition_model *model, const char *key)
{
	__attribute__nonnull__(model);

	return (struct oval_variable *)oval_string_map_get_value(model->variable_map, key);
}

void oval_definition_model_bind_variable_model(struct oval_definition_model *defmodel,
					       struct oval_variable_model *varmodel)
{
	//Bind values to all external variables specified in the variable model.
	struct oval_variable_iterator *variables = oval_definition_model_get_variables(defmodel);
	while (oval_variable_iterator_has_more(variables)) {
		struct oval_variable *variable = oval_variable_iterator_next(variables);
		if (oval_variable_get_type(variable) == OVAL_VARIABLE_EXTERNAL) {
			char *varid = oval_variable_get_id(variable);
			oval_datatype_t var_datatype = oval_variable_model_get_datatype(varmodel, varid);
			if (var_datatype) {	//values are bound in the variable model
				oval_datatype_t def_datatype = oval_variable_get_datatype(variable);
				if (def_datatype == var_datatype) {
					struct oval_string_iterator *values =
					    oval_variable_model_get_values(varmodel, varid);
					while (oval_string_iterator_has_more(values)) {
						char *text = oval_string_iterator_next(values);
						struct oval_value *value = oval_value_new(var_datatype, text);
						oval_variable_add_value(variable, value);
					}
					oval_string_iterator_free(values);
				} else {
					oscap_dprintf("WARNING: Unmatched variable datatypes"
						      "                             varid = %s; "
						      "         definition_model datatype = %s; "
						      "           variable_model datatype = %s; ",
						      varid,
						      oval_datatype_get_text(def_datatype),
						      oval_datatype_get_text(var_datatype));
				}
			}
		}
	}
	oval_variable_iterator_free(variables);
}

void oval_definition_model_clear_external_variables(struct oval_definition_model *model)
{
	struct oval_variable_iterator *vars_itr;

	vars_itr = oval_definition_model_get_variables(model);
	while (oval_variable_iterator_has_more(vars_itr)) {
		struct oval_variable *var;

		var = oval_variable_iterator_next(vars_itr);
		if (oval_variable_get_type(var) != OVAL_VARIABLE_EXTERNAL)
			continue;

		oval_variable_clear_values(var);
	}
	oval_variable_iterator_free(vars_itr);
}

struct oval_definition_iterator *oval_definition_model_get_definitions(struct oval_definition_model
								       *model)
{
	__attribute__nonnull__(model);

	struct oval_definition_iterator *iterator =
	    (struct oval_definition_iterator *)oval_string_map_values(model->definition_map);

	return iterator;
}

struct oval_test_iterator *oval_definition_model_get_tests(struct oval_definition_model *model)
{
	__attribute__nonnull__(model);

	struct oval_test_iterator *iterator = (struct oval_test_iterator *)oval_string_map_values(model->test_map);

	return iterator;
}

struct oval_object_iterator *oval_definition_model_get_objects(struct oval_definition_model *model)
{
	__attribute__nonnull__(model);

	struct oval_object_iterator *iterator =
	    (struct oval_object_iterator *)oval_string_map_values(model->object_map);

	return iterator;
}

struct oval_state_iterator *oval_definition_model_get_states(struct oval_definition_model *model)
{
	__attribute__nonnull__(model);

	struct oval_state_iterator *iterator = (struct oval_state_iterator *)oval_string_map_values(model->state_map);

	return iterator;
}

struct oval_variable_iterator *oval_definition_model_get_variables(struct oval_definition_model
								   *model)
{
	__attribute__nonnull__(model);

	struct oval_variable_iterator *iterator =
	    (struct oval_variable_iterator *)oval_string_map_values(model->variable_map);

	return iterator;
}

struct oval_definition *oval_definition_get_new(struct oval_definition_model *model, const char *id)
{
	struct oval_definition *definition = oval_definition_model_get_definition(model, id);
	if (definition == NULL) {
		definition = oval_definition_new(model, id);
		oval_definition_model_add_definition(model, definition);
	}
	return definition;
}

struct oval_variable *oval_variable_get_new(struct oval_definition_model *model, const char *id, oval_variable_type_t type)
{
	struct oval_variable *variable = oval_definition_model_get_variable(model, id);
	if (variable == NULL) {
		variable = oval_variable_new(model, id, type);
		oval_definition_model_add_variable(model, variable);
	} else {
		oval_variable_set_type(variable, type);
	}
	return variable;
}

struct oval_state *oval_state_get_new(struct oval_definition_model *model, const char *id)
{
	struct oval_state *state = oval_definition_model_get_state(model, id);
	if (state == NULL) {
		state = oval_state_new(model, id);
		oval_definition_model_add_state(model, state);
	}
	return state;
}

struct oval_object *oval_object_get_new(struct oval_definition_model *model, const char *id)
{
	struct oval_object *object = oval_definition_model_get_object(model, id);
	if (object == NULL) {
		object = oval_object_new(model, id);
		oval_definition_model_add_object(model, object);
	}
	return object;
}

struct oval_test *oval_test_get_new(struct oval_definition_model *model, const char *id)
{
	struct oval_test *test = oval_definition_model_get_test(model, id);
	if (test == NULL) {
		test = oval_test_new(model, id);
		oval_definition_model_add_test(model, test);
	}
	return test;
}

xmlNode *oval_definitions_to_dom(struct oval_definition_model *definition_model, xmlDocPtr doc, xmlNode * parent)
{

	xmlNodePtr root_node = NULL;

	if (parent) {
		root_node = xmlNewChild(parent, NULL, BAD_CAST "oval_definitions", NULL);
	} else {
		root_node = xmlNewNode(NULL, BAD_CAST "oval_definitions");
		xmlDocSetRootElement(doc, root_node);
	}
	xmlNewProp(root_node, BAD_CAST "xsi:schemaLocation", BAD_CAST definition_model->schema);

	xmlNs *ns_common = xmlNewNs(root_node, OVAL_COMMON_NAMESPACE, BAD_CAST "oval");
	xmlNs *ns_xsi = xmlNewNs(root_node, OVAL_XMLNS_XSI, BAD_CAST "xsi");
	xmlNs *ns_defntns = xmlNewNs(root_node, OVAL_DEFINITIONS_NAMESPACE, NULL);

	xmlSetNs(root_node, ns_common);
	xmlSetNs(root_node, ns_xsi);
	xmlSetNs(root_node, ns_defntns);

	/* Report generator */
	if (!parent) {
		xmlNode *tag_generator = xmlNewChild(root_node, ns_defntns, BAD_CAST "generator", NULL);
		_generator_to_dom(doc, tag_generator);
	}

	/* Report definitions */
	struct oval_definition_iterator *definitions = oval_definition_model_get_definitions(definition_model);
	if (oval_definition_iterator_has_more(definitions)) {
		xmlNode *definitions_node = NULL;
		while(oval_definition_iterator_has_more(definitions)) {
			struct oval_definition *definition = oval_definition_iterator_next(definitions);
			if (definitions_node == NULL) {
				definitions_node = xmlNewChild(root_node, ns_defntns, BAD_CAST "definitions", NULL);
			}
			oval_definition_to_dom(definition, doc, definitions_node);
		}
	}
        oval_definition_iterator_free(definitions);

	/* Report tests */
	struct oval_test_iterator *tests = oval_definition_model_get_tests(definition_model);
	if (oval_test_iterator_has_more(tests)) {
		xmlNode *tests_node = xmlNewChild(root_node, ns_defntns, BAD_CAST "tests", NULL);
		while (oval_test_iterator_has_more(tests)) {
			struct oval_test *test = oval_test_iterator_next(tests);
			oval_test_to_dom(test, doc, tests_node);
		}
	}
	oval_test_iterator_free(tests);

	/* Report objects */
	struct oval_object_iterator *objects = oval_definition_model_get_objects(definition_model);
	if (oval_object_iterator_has_more(objects)) {
		xmlNode *objects_node = xmlNewChild(root_node, ns_defntns, BAD_CAST "objects", NULL);
		while(oval_object_iterator_has_more(objects)) {
			struct oval_object *object = oval_object_iterator_next(objects);
			oval_object_to_dom(object, doc, objects_node);
		}
	}
	oval_object_iterator_free(objects);

	/* Report states */
	struct oval_state_iterator *states = oval_definition_model_get_states(definition_model);
	if (oval_state_iterator_has_more(states)) {
		xmlNode *states_node = xmlNewChild(root_node, ns_defntns, BAD_CAST "states", NULL);
		while (oval_state_iterator_has_more(states)) {
			struct oval_state *state = oval_state_iterator_next(states);
			oval_state_to_dom(state, doc, states_node);
		}
	}
	oval_state_iterator_free(states);

	/* Report variables */
	struct oval_variable_iterator *variables = oval_definition_model_get_variables(definition_model);
	if (oval_variable_iterator_has_more(variables)) {
		xmlNode *variables_node = xmlNewChild(root_node, ns_defntns, BAD_CAST "variables", NULL);
		while (oval_variable_iterator_has_more(variables)) {
			struct oval_variable *variable = oval_variable_iterator_next(variables);
			oval_variable_to_dom(variable, doc, variables_node);
		}
	}
	oval_variable_iterator_free(variables);

	return root_node;
}

int oval_definition_model_export(struct oval_definition_model *model, const char *file)
{

	__attribute__nonnull__(model);

	int retcode = 0;

	LIBXML_TEST_VERSION;

	xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
	if (doc == NULL) {
		oscap_setxmlerr(xmlGetLastError());
		return -1;
	}

	oval_definitions_to_dom(model, doc, NULL);
	retcode = xmlSaveFormatFileEnc(file, doc, "UTF-8", 1);

	if (retcode < 1)
		oscap_setxmlerr(xmlGetLastError());

	xmlFreeDoc(doc);

	return retcode;
}

int _generator_to_dom(xmlDocPtr doc, xmlNode * tag_generator)
{
	xmlNs *ns_common = xmlSearchNsByHref(doc, tag_generator, OVAL_COMMON_NAMESPACE);
	xmlNewChild(tag_generator, ns_common, BAD_CAST "product_name", BAD_CAST "OPEN SCAP");
	xmlNewChild(tag_generator, ns_common, BAD_CAST "schema_version", BAD_CAST "5.5");
	time_t epoch_time[] = { 0 };
	time(epoch_time);
	struct tm *lt = localtime(epoch_time);
	char timestamp[] = "yyyy-mm-ddThh:mm:ss";
	snprintf(timestamp, sizeof(timestamp), "%4d-%02d-%02dT%02d:%02d:%02d",
		 1900 + lt->tm_year, 1 + lt->tm_mon, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
	xmlNewChild(tag_generator, ns_common, BAD_CAST "timestamp", BAD_CAST timestamp);

	return 1;
}

