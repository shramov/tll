/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _CONFIG_YAML_H
#define _CONFIG_YAML_H

tll_config_t * yaml_load(std::string_view filename);
tll_config_t * yaml_load_data(std::string_view data);

#endif//_CONFIG_YAML_H
