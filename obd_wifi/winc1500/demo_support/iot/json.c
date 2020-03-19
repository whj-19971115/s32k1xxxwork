/*******************************************************************************
  File Name:
    json.c

  Summary:
    JSON parser

*******************************************************************************/

//DOM-IGNORE-BEGIN
/*==============================================================================
Copyright 2016 Microchip Technology Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <string.h>
#include <stdio.h>
//#include <errno.h>
#include "winc1500_api.h"
#include "demo_config.h"

#if defined(USING_SUPPORT_IOT)
#include "demo_support/iot/errno_local.h"
#include "demo_support/iot/json.h"

enum json_cmd
{
    JSON_CMD_NONE = 0,
    JSON_CMD_ENTER_OBJECT,
    JSON_CMD_EXIT_OBJECT,
    JSON_CMD_ENTER_ARRAY,
    JSON_CMD_EXIT_ARRAY
};


/**
 *    @token is pair of name : value.
 *    @param buffer             : [in] Read out line
 *    @param dest                : [out] destination buffer
 *    @param cmd                : [out] 0 : none 1: enter object 2: exit object 3: enter array 4: exit array
 *    @return                      : start point of next token
 */
static char * _json_read_token(char *buffer, uint32_t buffer_size, char *dest, uint32_t dest_size, enum json_cmd *cmd)
{
    uint32_t dest_index = 0;
    char in_quote = 0;

    *cmd = JSON_CMD_NONE;

    dest[0] = '\0';

    for (; buffer_size > 0; buffer_size--) 
    {
        char ch = *buffer++;
        if ( ch == '\"') 
        {
            if (in_quote == 0) 
            {
                in_quote = 1;
            } 
            else if (in_quote == 1) 
            {
                in_quote = 0;
            }
        }
        if (!in_quote) 
        {
            if (ch == ',') 
            {
                break;
            } 
            else if (ch == '}') 
            {
                *cmd = JSON_CMD_EXIT_OBJECT;
                break;
            } 
            else if (ch == ']') 
            {
                *cmd = JSON_CMD_EXIT_ARRAY;
                break;
            } 
            else if (ch == '\t' || ch == ' ' || ch == '\r' || ch == '\n') 
            {
                continue;
            } 
            else if (ch == '\0') 
            {
                return NULL;
            } 
            else if ( ch == '{') 
            {
                *cmd = JSON_CMD_ENTER_OBJECT;
                break;
            } 
            else if ( ch == '[') 
            {
                *cmd = JSON_CMD_ENTER_ARRAY;
                break;
            }
        }

        if (dest_index < dest_size)
        {
            dest[dest_index++] = ch;
        }
    }

    dest[dest_index] = '\0';

    return buffer;
}

static void _json_parse(char *data, char *ptr, enum json_cmd cmd, struct json_obj *out)
{
    int i;
    int minus = 0;
    int exponent = -1;
    int num = 0;
    int find_point = 0;
    int real_size = 1;
    int minus_exp = 0;

    if (data[0] != '\0') 
    {
        for (i = 0; *data != '\0' && *data != ':'; data++) 
        {
            if (*data != '\"' && i < JSON_MAX_NAME_SIZE - 1) 
            {
                out->name[i++] = *data;
            }
        }
        out->name[i] = '\0';
        out->type = JSON_TYPE_NULL;

        if (*data == ':') 
        {
            data++;
            if (*data == '-') 
            {
                minus = 1;
                data++;
            }
            if (*data == '\"') 
            {
                out->type = JSON_TYPE_STRING;
                for (i = 0; *data != '\0'; data++) 
                {
                    if (*data != '\"' && i < JSON_MAX_TOKEN_SIZE - JSON_MAX_NAME_SIZE - 1) 
                    {
                        out->value.s[i++] = *data;
                    }
                }
                out->value.s[i] = '\0';
            } 
            else if (*data >= '0' && *data <= '9') 
            {
                for (; *data != '\0'; i++) 
                {
                    if (*data >= '0' && *data <= '9') 
                    {
                        if (exponent < 0) 
                        {
                            num = num * 10 + (*data & 0xF);
                            if (find_point == 1) 
                            {
                                real_size = 10;
                            }
                        } 
                        else 
                        {
                            exponent = exponent * 10 + *data - '0';
                        }
                    } 
                    else if (*data == '.') 
                    {
                        find_point = 1;
                    } 
                    else if (*data == 'e' || *data == 'E') 
                    {
                        exponent = 0;
                    } 
                    else if (*data == '-' && exponent >= 0) 
                    {
                        minus_exp = 1;
                    }
                }

                if (find_point == 0 && exponent < 0)
                {
                    out->type = JSON_TYPE_INTEGER;
                    out->value.i = (minus) ? num * -1 : num;
                }
                else
                {
                    out->type = JSON_TYPE_REAL;
                    out->value.d = (minus) ? (double)num / (double)real_size * -1 : (double)num / (double)real_size;
                    if (exponent >= 0)
                    {
                        if (minus_exp)
                        {
                            for (i = 0; i < exponent; i++)
                            {
                                out->value.d /= 10.0f;
                            }
                        }
                        else
                        {
                            for (i = 0; i < exponent; i++)
                            {
                                out->value.d *= 10.0f;
                            }
                        }
                    }
                }
            }
            else if (!strncmp(data, "true", 4))
            {
                out->type = JSON_TYPE_BOOLEAN;
                out->value.b = 1;

            }
            else if (!strncmp(data, "false", 5))
            {
                out->type = JSON_TYPE_BOOLEAN;
                out->value.b = 0;

            }
            else if (*data == '\0' || !strncmp(data, "null", 4))
            {
                out->type = JSON_TYPE_NULL;
            } 
            else
            {
                out->type = JSON_TYPE_STRING;
                for (i = 0; *data != '\0'; data++)
                {
                    if (*data != '\"' && i < JSON_MAX_TOKEN_SIZE - JSON_MAX_NAME_SIZE - 1) 
                    {
                        out->value.s[i++] = *data;
                    }
                }
                out->value.s[i] = '\0';
            }
        }
    }
    else
    {
        out->name[0] = 0;
    }

    if (cmd == JSON_CMD_ENTER_OBJECT)
    {
        out->type = JSON_TYPE_OBJECT;
        for (; *ptr != '{'; ptr--);
        out->value.o = ptr;
    }
    else if (cmd == JSON_CMD_ENTER_ARRAY)
    {
        out->type = JSON_TYPE_ARRAY;
        for (; *ptr != '['; ptr--);
        out->value.o = ptr;
    }
}

int json_create(struct json_obj *obj, const char *data, int data_len)
{
    if (obj == NULL || data == NULL)
    {
        return -EINVAL;
    }
    for (; *data != '{'; data++, data_len--)
    {
        if (*data == '\0')
        {
            return -EINVAL;
        }
    }

    obj->type = JSON_TYPE_OBJECT;
    obj->name[0] = 0;
    obj->value.o = (char *)data;
    obj->end_ptr = (char *)data + data_len;

    return 0;
}

int json_get_child_count(struct json_obj *obj)
{
    char dest[2];
    enum json_cmd cmd;
    int child = 0;
    char *ptr;
    int depth = -1;

    if (obj == NULL || (obj->type != JSON_TYPE_OBJECT && obj->type != JSON_TYPE_ARRAY) ||
        obj->value.o == NULL)
    {
        return -EINVAL;
    }

    for (ptr = obj->value.o; ; )
    {
        ptr = _json_read_token(ptr, obj->end_ptr - ptr, dest, 1, &cmd);
        if (ptr == NULL)
        {
            break;
        }
        if (dest[0] == '\0' && cmd == JSON_CMD_NONE)
        {
            continue;
        }
        if (cmd == JSON_CMD_EXIT_ARRAY || cmd == JSON_CMD_EXIT_OBJECT)
        {
            depth--;
        }
        else
        {
            if (depth == 0)
            {
                /* Found members */
                child++;
            }
            if (cmd == JSON_CMD_ENTER_OBJECT || cmd == JSON_CMD_ENTER_ARRAY)
            {
                depth++;
            }
        }
    }

    return child;
}

int json_get_child(struct json_obj *obj, int index, struct json_obj *out)
{
    char dest[JSON_MAX_TOKEN_SIZE];
    enum json_cmd cmd;
    int child = 0;
    char *ptr;
    int depth = -1;

    if (obj == NULL || out == NULL || (obj->type != JSON_TYPE_OBJECT && obj->type != JSON_TYPE_ARRAY) ||
        obj->value.o == NULL)
    {
        return -EINVAL;
    }

    for (ptr = obj->value.o; ; )
    {
        ptr = _json_read_token(ptr, obj->end_ptr - ptr, dest, JSON_MAX_TOKEN_SIZE - 1, &cmd);
        if (ptr == NULL)
        {
            break;
        }
        if (dest[0] == '\0' && cmd == JSON_CMD_NONE)
        {
            continue;
        }
        if (cmd == JSON_CMD_EXIT_ARRAY || cmd == JSON_CMD_EXIT_OBJECT)
        {
            depth--;
        }
        else
        {
            if (depth == 0)
            {
                /* Found members */
                if (child == index)
                {
                    _json_parse(dest, ptr, cmd, out);
                    out->end_ptr = obj->end_ptr;
                    break;
                }
                child++;
            }
            if (cmd == JSON_CMD_ENTER_OBJECT || cmd == JSON_CMD_ENTER_ARRAY)
            {
                depth++;
            }
        }
    }

    return child;
}

int json_find(struct json_obj *obj, const char *name, struct json_obj *out)
{
    char dest[JSON_MAX_TOKEN_SIZE];
    enum json_cmd cmd;
    char *ptr;
    char *name_ptr = (char *)name;
    int depth = -1;

    if (obj == NULL || out == NULL || (obj->type != JSON_TYPE_OBJECT && obj->type != JSON_TYPE_ARRAY) ||
        obj->value.o == NULL || name == NULL || strlen(name) == 0)
    {
        return -EINVAL;
    }

    for (ptr = obj->value.o; ; )
    {
        ptr = _json_read_token(ptr, obj->end_ptr - ptr, dest, JSON_MAX_TOKEN_SIZE - 1, &cmd);
        if (ptr == NULL)
        {
            break;
        }
        if (dest[0] == '\0' && cmd == JSON_CMD_NONE)
        {
            continue;
        }

        if (depth == 0)
        {
            _json_parse(dest, ptr, cmd, out);
            if (!strncmp(name_ptr, out->name, strlen(out->name)) &&
                (name_ptr[strlen(out->name)] == ':' || name_ptr[strlen(out->name)] == '\0'))
            {
                if (name_ptr[strlen(out->name)] == '\0')
                {
                    return 0;
                }
                else
                {
                    name_ptr += strlen(out->name) + 1;
                    depth = 0;
                    continue;
                }
            }
        }

        if (cmd == JSON_CMD_EXIT_ARRAY || cmd == JSON_CMD_EXIT_OBJECT)
        {
            if (depth == 0)
            {
                return -1;
            }
            depth--;
        }
        else if (cmd == JSON_CMD_ENTER_OBJECT || cmd == JSON_CMD_ENTER_ARRAY)
        {
            depth++;
        }
    }

    return -1;
}

#endif