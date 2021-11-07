



struct tokenizer {
	char	*start_data;
	char	*data;
	usize	size;
	usize	cap;
	char	*current;
	char	*prev;
};

enum token {
	Token_eof,
	Token_newline,
	Token_identifier,
	Token_preprocessing_number,
	Token_string,
	Token_character,
	Token_punctuator,
	Token_path_global,
	Token_path_relative,
	Token_link,
};

void	push_tokenizer_byte (struct tokenizer *tokenizer, int byte) {
	tokenizer->data[tokenizer->size] = byte;
	tokenizer->size += 1;
}

void	push_tokenizer_bytes (struct tokenizer *tokenizer, void *bytes, usize length) {
	memcpy (tokenizer->data + tokenizer->size, bytes, length);
	tokenizer->size += length;
}

int		prepare_tokenizer (struct tokenizer *tokenizer, usize tofit) {
	int		success;

	if (tokenizer->size + tofit + (2 + 1 + sizeof (void *)) > tokenizer->cap) {
		void	*memory;

		memory = expand_array (0, &tokenizer->cap);
		if (memory) {
			if (!tokenizer->start_data) {
				tokenizer->start_data = memory;
			}
			if (tokenizer->data) {
				*(void **) tokenizer->data = memory;
				push_tokenizer_byte (tokenizer, 0);
				push_tokenizer_byte (tokenizer, 0);
				push_tokenizer_byte (tokenizer, Token_link);
				tokenizer->prev = tokenizer->current;
				tokenizer->current = tokenizer->data + tokenizer->size;
				push_tokenizer_bytes (tokenizer, (char [sizeof memory]) {0}, sizeof memory);
				*(void **) tokenizer->current = memory;
			}
			tokenizer->data = memory;
			tokenizer->size = sizeof memory;
			*(void **) tokenizer->data = 0;
			success = 1;
		} else {
			Error ("NO MEMORY!!!!!!!!!!!!!!!!");
			success = 0;
		}
	} else {
		success = 1;
	}
	return (success);
}

int		get_token_position (const char *token) {
	int		position;

	position = (unsigned) token[-2];
	position += (unsigned) token[-3] << 8;
	return (position);
}

int		push_token (struct tokenizer *tokenizer, int position, int token, const char *data, usize length) {
	int		success;

	if ((success = prepare_tokenizer (tokenizer, length + 4))) {
		push_tokenizer_byte (tokenizer, (position & 0xFF00) >> 8 );
		push_tokenizer_byte (tokenizer, position & 0xFF);
		push_tokenizer_byte (tokenizer, token);
		tokenizer->prev = tokenizer->current;
		tokenizer->current = tokenizer->data + tokenizer->size;
		push_tokenizer_bytes (tokenizer, (void *) data, length);
		push_tokenizer_byte (tokenizer, 0);
	}
	return (success);
}

int		push_newline_token (struct tokenizer *tokenizer) {
	int		success;

	if (tokenizer->current && tokenizer->current[-1] == Token_newline && tokenizer->current[0] < 0x7f) {
		tokenizer->current[0] += 1;
		success = 1;
	} else {
		if ((success = prepare_tokenizer (tokenizer, 5))) {
			push_tokenizer_byte (tokenizer, 0);
			push_tokenizer_byte (tokenizer, 0);
			push_tokenizer_byte (tokenizer, Token_newline);
			tokenizer->prev = tokenizer->current;
			tokenizer->current = tokenizer->data + tokenizer->size;
			push_tokenizer_byte (tokenizer, 1);
			push_tokenizer_byte (tokenizer, 0);
		}
	}
	return (success);
}

int		push_compiled_newline_token (struct tokenizer *tokenizer, int count) {
	int		success = 1;

	while (count > 0 && success) {
		success = push_newline_token (tokenizer);
		count -= 1;
	}
	return (success);
}

int		push_string_token (struct tokenizer *tokenizer, int position, const char *string, usize length, int push_at_existing) {
	int		success;

	if (push_at_existing && tokenizer->current && tokenizer->current[-1] == Token_string) {
		tokenizer->size -= 1;
		if ((success = prepare_tokenizer (tokenizer, length + 1))) {
			push_tokenizer_bytes (tokenizer, (void *) string, length);
			push_tokenizer_byte (tokenizer, 0);
		}
	} else if ((success = prepare_tokenizer (tokenizer, length + 4))) {
		push_tokenizer_byte (tokenizer, (position & 0xFF00) >> 8 );
		push_tokenizer_byte (tokenizer, position & 0xFF);
		push_tokenizer_byte (tokenizer, Token_string);
		tokenizer->prev = tokenizer->current;
		tokenizer->current = tokenizer->data + tokenizer->size;
		push_tokenizer_bytes (tokenizer, (void *) string, length);
		push_tokenizer_byte (tokenizer, 0);
	}
	return (success);
}

const char	*next_const_token (const char *tokens) {
	while (*tokens) {
		tokens += 1;
	}
	tokens += 1; /* skip null-term */
	tokens += 2; /* skip position */
	tokens += 1; /* skip token kind */
	if (tokens[-1] == Token_link) {
		void	*memory = *(void **) tokens;

		tokens = memory;
		tokens += sizeof memory; /* skip next data pointer */
		tokens += 2; /* skip position */
		tokens += 1; /* skip token kind */
	}
	return (tokens);
}

char	*next_token (char *tokens) {
	return ((char *) next_const_token (tokens));
}

static unsigned	escape_symbol (const char **psource) {
	const char	*source = *psource;
	unsigned	result = *source;

	if (*source == '\\') {
		source += 1;
		switch (*source) {
			case 'a': result = '\a'; break ;
			case 'b': result = '\b'; break ;
			case 'f': result = '\f'; break ;
			case 'n': result = '\n'; break ;
			case 'r': result = '\r'; break ;
			case 't': result = '\t'; break ;
			case 'v': result = '\v'; break ;
			case '\\': result = '\\'; break ;
			case '\'': result = '\''; break ;
			case '\"': result = '\"'; break ;
			case '\?': result = '\?'; break ;
			case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': {
				unsigned value = '0' - *source;
				if (source[1] >= '0' && source[1] <= '7') {
					source += 1;
					value *= 8;
					value += '0' - *source;
					if (source[1] >= '0' && source[1] <= '7') {
						source += 1;
						value *= 8;
						value += '0' - *source;
					}
				}
				result = (char) value;
			} break ;
			case 'x': {
				unsigned value = 0;
				while (isxdigit (source[1])) {
					source += 1;
					value *= 16;
					if (*source >= '0' && *source <= '9') {
						value += '0' - *source;
					} else if (*source >= 'a' && *source <= 'f') {
						value += 10 + ('a' - *source);
					} else if (*source >= 'A' && *source <= 'F') {
						value += 10 + ('A' - *source);
					}
				}
				result = value;
			} break ;
			case 'u': case 'U': {
				Error ("Unicode escape sequences are not implemented");
			} break ;
		}
		*psource = source;
	} else {
		result = *source;
	}
	return (result);
}

static int	unescape_symbol (int ch, char **pout) {
	char		*out = *pout;

	switch (ch) {
		case '\a': *out++ = '\\'; *out++ = 'a'; break ;
		case '\b': *out++ = '\\'; *out++ = 'b'; break ;
		case '\f': *out++ = '\\'; *out++ = 'f'; break ;
		case '\n': *out++ = '\\'; *out++ = 'n'; break ;
		case '\r': *out++ = '\\'; *out++ = 'r'; break ;
		case '\t': *out++ = '\\'; *out++ = 't'; break ;
		case '\v': *out++ = '\\'; *out++ = 'v'; break ;
		case '\\': *out++ = '\\'; *out++ = '\\'; break ;
		case '\'': *out++ = '\\'; *out++ = '\''; break ;
		case '\"': *out++ = '\\'; *out++ = '\"'; break ;
		case '\?': *out++ = '\\'; *out++ = '\?'; break ;
		default: {
			if (isprint (ch)) {
				*out++ = ch;
			} else {
				*out++ = '\\';
				while (ch) {
					*out++ = '0' + (ch & 0x7);
					ch >>= 3;
				}
			}
		}
	}
	*pout = out;
	return (1);
}

char	*get_first_token (struct tokenizer *tokenizer) {
	return (tokenizer->start_data ? tokenizer->start_data + sizeof (void *) + 3 : 0);
}

int		end_tokenizer (struct tokenizer *tokenizer) {
	int		success;

	if ((success = tokenizer->cap - tokenizer->size >= 5 || prepare_tokenizer (tokenizer, 5))) {
		push_tokenizer_byte (tokenizer, 0);
		push_tokenizer_byte (tokenizer, 0);
		push_tokenizer_byte (tokenizer, Token_eof);
		tokenizer->prev = tokenizer->current;
		tokenizer->current = tokenizer->data + tokenizer->size;
		push_tokenizer_byte (tokenizer, 0);
	}
	return (success);
}

struct token_state {
	const char	*line_start;
	int			line;
	int			check_include;
	int			its_include;
	int			*nl_array;
	const char	*filename;
};

int		make_token (struct tokenizer *tokenizer, struct token_state *state, const char **pcontent) {
	int			success = 1;
	const char	*content = *pcontent;

	while (*content && success && (isspace (*content) || *content <= 0x20)) {
		if (*content == '\n') {
			success = push_newline_token (tokenizer);
			while (*state->nl_array && *state->nl_array == state->line && success) {
				success = push_newline_token (tokenizer);
				state->nl_array += 1;
			}
			state->line += 1;
			state->check_include = 0;
			state->line_start = content + 1;
		}
		content += 1;
	}
	if (!*content || !success) {
	} else if (isalpha (*content) || *content == '_') {
		const char	*start = content;
		int			position = content - state->line_start;

		do {
			content += 1;
		} while (isalnum (*content) || *content == '_');
		push_token (tokenizer, position, Token_identifier, start, content - start);
		state->its_include = 0;
		if (state->check_include && 0 == strncmp (start, "include", content - start)) {
			state->its_include = 1;
		}
		state->check_include = 0;
	} else if (isdigit (*content) || (*content == '.' && isdigit (content[1]))) {
		const char	*start = content;
		int			position = content - state->line_start;

		content += (*content == '.');
		do {
			content += 1 + ((*content == 'e' || *content == 'E' || *content == 'p' || *content == 'P') && (content[1] == '+' || content[1] == '-'));
		} while (isalnum (*content) || *content == '_' || *content == '.');
		push_token (tokenizer, position, Token_preprocessing_number, start, content - start);
		state->check_include = 0; state->its_include = 0;
	} else if (*content == '"' || *content == '\'' || (state->its_include && *content == '<')) {
		char	buffer_memory[256], *buffer = buffer_memory;
		char	end_symbol = (*content == '<' ? '>' : *content);
		int		pushed = 0;
		int		position;

		content += 1;
		position = content - state->line_start;
		while (*content && *content != end_symbol && success) {
			if (buffer - buffer_memory >= (isize) sizeof buffer_memory) {
				if ((success = push_string_token (tokenizer, position, buffer_memory, sizeof buffer_memory, pushed))) {
					pushed = 1;
					buffer = buffer_memory;
				} else {
					break ;
				}
			}
			int	is_forbidden_newline = (*content == '\n');
			if (*content == '\\' && !state->its_include) {
				is_forbidden_newline = is_forbidden_newline || (content[1] == '\n');
				*buffer++ = escape_symbol (&content);
			} else {
				*buffer++ = *content;
			}
			if (is_forbidden_newline) {
				/* TODO: test it */
				Error_Message (state->filename, state->line, position, "new line character in the string literal is forbidden");
				success = 0;
				break ;
			}
			content += 1;
		};
		if (success) {
			success = push_string_token (tokenizer, position, buffer_memory, buffer - buffer_memory, pushed);
			if (end_symbol == '\'') {
				tokenizer->current[-1] = Token_character;
			} else if (end_symbol == '>') {
				tokenizer->current[-1] = Token_path_global;
			} else if (state->its_include) {
				tokenizer->current[-1] = Token_path_relative;
			}
			content += 1;
		}
		state->check_include = 0; state->its_include = 0;
	} else {
		static const char	*const strings[] = {
			"++", "+=", "--", "-=", "->", "...", "!=", "*=", "&&", "&=", "/=", "%=", "<=", "<<=", "<<", ">=", ">>=", ">>", "^=", "|=", "||", "==", "##",
			"<%", "%>", "<:", ":>", "%:%:", "%:",
			0
		};
		const char	*const *string = strings;
		usize		length = 1;

		while (*string) {
			usize	string_length = strlen (*string);

			if (0 == strncmp (*string, content, string_length)) {
				length = string_length;
				break ;
			}
			string += 1;
		}
		state->check_include = 0; state->its_include = 0;
		if (length == 1 && *content == '#' && ((tokenizer->current && tokenizer->current[-1] == Token_newline) || !tokenizer->current)) {
			state->check_include = 1;
		}
		if (length == 2 && (*content == '<' || *content == '%' || *content == ':')) {
			if (0 == strncmp (content, "<%", 2)) {
				push_token (tokenizer, content - state->line_start, Token_punctuator, "{", 1);
			} else if (0 == strncmp (content, "%>", 2)) {
				push_token (tokenizer, content - state->line_start, Token_punctuator, "}", 1);
			} else if (0 == strncmp (content, "<:", 2)) {
				push_token (tokenizer, content - state->line_start, Token_punctuator, "[", 1);
			} else if (0 == strncmp (content, ":>", 2)) {
				push_token (tokenizer, content - state->line_start, Token_punctuator, "]", 1);
			} else if (0 == strncmp (content, "%:", 2)) {
				push_token (tokenizer, content - state->line_start, Token_punctuator, "#", 1);
			} else {
				push_token (tokenizer, content - state->line_start, Token_punctuator, content, length);
			}
		} else if (length == 4 && 0 == strncmp (content, "%:%:", 4)) {
			push_token (tokenizer, content - state->line_start, Token_punctuator, "##", 2);
		} else {
			push_token (tokenizer, content - state->line_start, Token_punctuator, content, length);
		}
		content += length;
	}
	*pcontent = content;
	return (success);
}

void	free_tokens (const char *tokens) {
	void	**memory = (void **) (tokens - 3 - sizeof (void *));

	while (*memory) {
		void	*next = *memory;

		free (memory);
		memory = (void **) next;
	}
	free (memory);
}

void	free_tokenizer (struct tokenizer *tokenizer) {
	if (tokenizer->start_data) {
		free_tokens (get_first_token (tokenizer));
	}
	memset (tokenizer, 0, sizeof *tokenizer);
}

char	*tokenize (const char *content, int *nl_array, const char *filename) {
	struct tokenizer	ctokenizer = {0}, *tokenizer = &ctokenizer;
	int					success = 1;
	struct token_state	state = {
		.check_include = 0,
		.its_include = 0,
		.line_start = content,
		.line = 1,
		.nl_array = nl_array,
		.filename = filename,
	};

	while (*content && success) {
		success = make_token (tokenizer, &state, &content);
	}
	if (success) {
		if (tokenizer->current && tokenizer->current[-1] != Token_newline) {
			success = push_newline_token (tokenizer);
		}
		if (success) {
			success = end_tokenizer (tokenizer);
		}
	}
	if (!success) {
		Error ("unsuccessful tokenization");
		free_tokenizer (tokenizer);
	}
	return (get_first_token (tokenizer));
}

int		unescape_string_token (const char *string, char *out, usize cap, usize *size) {
	usize	index = 0;
	char	*out_start = out;
	int		success = 1;

	*out++ = '\"';
	while (success && *string && (success = out - out_start < (isize) cap - 4)) {
		success = unescape_symbol ((unsigned char) *string, &out);
		string += 1;
	}
	if (success) {
		if (out - out_start < (isize) cap - 1) {
			*out++ = '\"';
			*size = out - out_start;
			*out++ = 0;
		} else {
			Error ("not enough memory");
			success = 0;
		}
	}
	return (success);
}

int		concatenate_token (struct tokenizer *tokenizer, const char *token, int line, const char *filename) {
	int		success;

	if (tokenizer->current && tokenizer->current[-1] && tokenizer->current[-1] != Token_newline && token[-1] && token[-1] != Token_newline) {
		char	*content;
		usize	cap = 0;

		content = expand_array (0, &cap);
		if (content) {
			usize	size = 0;

			if (tokenizer->current[-1] == Token_string) {
				success = unescape_string_token (tokenizer->current, content, cap, &size);
			} else {
				size = stpcpy (content, tokenizer->current) - content;
				if (size < cap) {
					if (token[-1] == Token_string) {
						usize	new_size = 0;

						success = unescape_string_token (token, content + size, cap - size, &new_size);
						size += new_size;
					} else {
						size = stpcpy (content + size, token) - content;
						if (size < cap) {
							content[size] = 0;
							success = 1;
						} else {
							Error ("not enough memory");
							success = 0;
						}
					}
				} else {
					Error ("not enough memory");
					success = 0;
				}
			}
		} else {
			success = 0;
		}
		if (success) {
			struct token_state	state = {
				.check_include = 0,
				.its_include = 0,
				.line_start = content,
				.line = 1,
				.nl_array = &(int) {0},
				.filename = filename,
			};
			const char	*ptr = content;

			if (tokenizer->prev) {
				tokenizer->size -= strlen (tokenizer->current) + 1 + 3;
				tokenizer->current = tokenizer->prev;
				tokenizer->prev = 0;
				success = make_token (tokenizer, &state, (const char **) &ptr);
				if (success) {
					if (!(success = (*ptr == 0))) {
						Error_Message (filename, line, get_token_position (token), "resulting token is invalid");
						success = 0;
					}
				}
			} else {
				Error ("prev pointer is null");
				success = 0;
			}
		}
		free (content);
	} else {
		Error ("invalid tokens");
		success = 0;
	}
	return (success);
}

#ifndef Without_Tests
void	test_tokenize_stage (void) {
	usize	size;
	const char	*filename = "test.c";
	char	*content = read_entire_file (filename, &size);
	char	*tokens;
	int		*newline_array;

	preprocess_text (content, content + size, &newline_array);
	if (content) {
		Debug ("tokenizing content:\n%s\n--------------", content);
		tokens = tokenize (content, newline_array, filename);
		if (tokens) {
			char	*start = tokens;

			while (tokens[-1]) {
				if (tokens[-1] != Token_newline) <%
					Debug ("%s", tokens);
				%> else {
					fprintf (stderr, "%d\n", (int) tokens[0]);
				}
				(void) tokens<:0:>;
				tokens = next_token (tokens);
			}
			free_tokens (start);
		} else {
			Error ("no tokens");
		}
		free (content);
	} else {
		Error ("no content");
	}

}
#endif

int		copy_token (struct tokenizer *tokenizer, const char *token) {
	usize	length;
	int		success;

	length = strlen (token);
	if (token[-1] == Token_newline && tokenizer->current && tokenizer->current[-1] == Token_newline) {
		if (token[0] + tokenizer->current[0] > 0x7f) {
			int		remaining = (token[0] + tokenizer->current[0]) - 0x7f;

			tokenizer->current[0] = 0x7f;
			if ((success = prepare_tokenizer (tokenizer, length + 4))) {
				push_tokenizer_byte (tokenizer, 0);
				push_tokenizer_byte (tokenizer, 0);
				push_tokenizer_byte (tokenizer, token[-1]);
				tokenizer->prev = tokenizer->current;
				tokenizer->current = tokenizer->data + tokenizer->size;
				push_tokenizer_byte (tokenizer, remaining);
				push_tokenizer_byte (tokenizer, 0);
			}
		} else {
			tokenizer->current[0] += token[0];
			success = 1;
		}
	} else if ((success = prepare_tokenizer (tokenizer, length + 4))) {
		push_tokenizer_byte (tokenizer, token[-3]);
		push_tokenizer_byte (tokenizer, token[-2]);
		push_tokenizer_byte (tokenizer, token[-1]);
		tokenizer->prev = tokenizer->current;
		tokenizer->current = tokenizer->data + tokenizer->size;
		push_tokenizer_bytes (tokenizer, (void *) token, length);
		push_tokenizer_byte (tokenizer, 0);
	}
	return (success);
}

