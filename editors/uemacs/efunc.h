/*	efunc.h
 *
 *	Function declarations and names.
 *
 *	This file list all the C code functions used and the names to use
 *      to bind keys to them. To add functions,	declare it here in both the
 *      extern function list and the name binding table.
 *
 *	modified by Petri Kutvonen
 */

/* External function declarations. */

/* word.c */
extern Task<int> cmd_wrap_word(int f, int n);
extern Task<int> cmd_previous_word(int f, int n);
extern Task<int> cmd_next_word(int f, int n);
extern Task<int> cmd_case_word_upper(int f, int n);
extern Task<int> cmd_case_word_lower(int f, int n);
extern Task<int> cmd_case_word_capitalize(int f, int n);
extern Task<int> cmd_delete_next_word(int f, int n);
extern Task<int> cmd_delete_previous_word(int f, int n);
extern int inword(void);
extern Task<int> cmd_fill_paragraph(int f, int n);
extern Task<int> cmd_justify_paragraph(int f, int n);
extern Task<int> cmd_kill_paragraph(int f, int n);
extern Task<int> cmd_count_words(int f, int n);

/* window.c */
extern Task<int> cmd_split_current_window(int f, int n);
extern Task<int> cmd_next_window(int f, int n);
extern Task<int> cmd_previous_window(int f, int n);
extern Task<int> cmd_delete_other_windows(int f, int n);
extern Task<int> cmd_delete_window(int f, int n);
extern Task<int> cmd_grow_window(int f, int n);
extern Task<int> cmd_shrink_window(int f, int n);
extern Task<int> cmd_move_window_up(int f, int n);
extern Task<int> cmd_move_window_down(int f, int n);
extern Task<int> cmd_resize_window(int f, int n);
extern Task<int> cmd_scroll_next_up(int f, int n);
extern Task<int> cmd_scroll_next_down(int f, int n);
extern Task<int> cmd_save_window(int f, int n);
extern Task<int> cmd_restore_window(int f, int n);
extern Task<int> cmd_redraw_display(int f, int n);
extern Task<int> cmd_clear_and_redraw(int f, int n);
extern Task<int> cmd_change_screen_size(int f, int n);
extern Task<int> cmd_change_screen_width(int f, int n);

/* basic.c */
/* The character and line motions, as plain functions: they are pure
   computation, and every other motion is written in terms of them one step at
   a time.  A co_await is a call and not a tail call here, so awaiting one per
   character would grow the native stack by the length of the line. */
extern int forwchar(int f, int n);
extern int backchar(int f, int n);
extern int forwline(int f, int n);
extern int backline(int f, int n);
extern Task<int> cmd_beginning_of_line(int f, int n);
extern Task<int> cmd_backward_character(int f, int n);
extern Task<int> cmd_end_of_line(int f, int n);
extern Task<int> cmd_forward_character(int f, int n);
extern Task<int> cmd_goto_line(int f, int n);
extern Task<int> cmd_beginning_of_file(int f, int n);
extern Task<int> cmd_end_of_file(int f, int n);
extern Task<int> cmd_next_line(int f, int n);
extern Task<int> cmd_previous_line(int f, int n);
extern Task<int> cmd_previous_paragraph(int f, int n);
extern Task<int> cmd_next_paragraph(int f, int n);
extern Task<int> cmd_next_page(int f, int n);
extern Task<int> cmd_previous_page(int f, int n);
extern Task<int> cmd_set_mark(int f, int n);
extern Task<int> cmd_exchange_point_and_mark(int f, int n);

/* random.c */
extern int tabsize; /* Tab size (0: use real tabs). */
extern Task<int> cmd_set_fill_column(int f, int n);
extern Task<int> cmd_buffer_position(int f, int n);
extern int getcline(void);
extern int getccol(int bflg);
extern int setccol(int pos);
extern Task<int> cmd_transpose_characters(int f, int n);
extern Task<int> cmd_quote_character(int f, int n);
extern Task<int> cmd_handle_tab(int f, int n);
extern Task<int> cmd_detab_line(int f, int n);
extern Task<int> cmd_entab_line(int f, int n);
extern Task<int> cmd_trim_line(int f, int n);
extern Task<int> cmd_open_line(int f, int n);
extern Task<int> cmd_newline(int f, int n);
extern Task<int> cinsert(void);
extern Task<int> insbrace(int n, int c);
extern Task<int> inspound(void);
extern Task<int> cmd_delete_blank_lines(int f, int n);
extern Task<int> cmd_newline_and_indent(int f, int n);
extern Task<int> cmd_delete_next_character(int f, int n);
extern Task<int> cmd_delete_previous_character(int f, int n);
extern Task<int> cmd_kill_to_end_of_line(int f, int n);
extern Task<int> cmd_add_mode(int f, int n);
extern Task<int> cmd_delete_mode(int f, int n);
extern Task<int> cmd_add_global_mode(int f, int n);
extern Task<int> cmd_delete_global_mode(int f, int n);
extern Task<int> adjustmode(int kind, int global);
extern Task<int> cmd_clear_message_line(int f, int n);
extern Task<int> cmd_write_message(int f, int n);
extern Task<int> cmd_goto_matching_fence(int f, int n);
extern Task<int> fmatch(int ch);
extern Task<int> cmd_insert_string(int f, int n);
extern Task<int> cmd_overwrite_string(int f, int n);

/* main.c */
extern Task<void> edinit(char *bname);
extern Task<int> execute(int c, int f, int n);
extern Task<int> cmd_quick_exit(int f, int n);
extern Task<int> cmd_exit_emacs(int f, int n);
extern Task<int> cmd_begin_macro(int f, int n);
extern Task<int> cmd_end_macro(int f, int n);
extern Task<int> cmd_execute_macro(int f, int n);
extern Task<int> cmd_abort_command(int f, int n);
extern int readonly_error(void);
extern int restricted_error(void);
extern Task<int> cmd_nop(int f, int n);
extern Task<int> cmd_meta_prefix(int f, int n);
extern Task<int> cmd_ctlx_prefix(int f, int n);
extern Task<int> cmd_universal_argument(int f, int n);

/* display.c */
extern Task<void> display_open(void);
extern Task<void> display_close(void);
extern Task<int> cmd_update_screen(int f, int n);
extern Task<void> update(void);
extern Task<void> update_now(void);
extern void update_modeline(void);
extern void movecursor(int row, int col);
extern void msg_erase(void);
extern void msg_printf(const char *fmt, ...);
extern void msg_force(char *s);
extern void msg_append(const char *s);
extern void msg_puts(const char *s);
extern Task<void> checkwinsize(void);

/* region.c */
extern Task<int> cmd_kill_region(int f, int n);
extern Task<int> cmd_copy_region(int f, int n);
extern Task<int> cmd_case_region_lower(int f, int n);
extern Task<int> cmd_case_region_upper(int f, int n);
extern int getregion(struct region *rp);

/* screen.cpp -- was tcap.c and posix.c */
extern Task<void> tcapopen(void);
extern Task<void> tcapclose(void);
extern void tcapkopen(void);
extern void tcapkclose(void);
extern void tcapmove(int row, int col);
extern void tcapeeol(void);
extern void tcapeeop(void);
extern void tcapbeep(void);
extern void tcaprev(int state);
extern Task<void> ttopen(void);
extern Task<void> ttclose(void);
extern int ttputc(int c);
extern void ttflush(void);
extern void ttpause(void);
extern Task<int> ttgetc(void);
extern void ttungetc(int c);
extern int typahead(void);
extern Task<void> tcappause(const char *prompt);
extern void getscreensize(int *widthp, int *heightp);

/* input.c */
extern Task<int> ask_yesno(char *prompt);
extern Task<int> ask_string(char *prompt, char *buf, int nbuf);
extern Task<int> ask_string_until(char *prompt, char *buf, int nbuf, int eolchar);
extern int keycode_to_char(int c);
extern int char_to_keycode(int c);
extern Task<fn_t> getname(void);
extern Task<int> tgetc(void);
extern Task<int> get1key(void);
extern Task<int> getcmd(void);
extern Task<int> getstring(char *prompt, char *buf, int nbuf, int eolchar);
extern void outstring(char *s);
extern void ostring(char *s);

/* bind.c */
extern Task<int> cmd_help(int f, int n);
extern Task<int> cmd_describe_bindings(int f, int n);
extern Task<int> cmd_apropos(int f, int n);
extern Task<int> cmd_describe_key(int f, int n);
extern Task<int> cmd_bind_to_key(int f, int n);
extern Task<int> cmd_unbind_key(int f, int n);
extern int unbindchar(int c);
extern Task<unsigned int> getckey(int mflag);
extern Task<int> startup(char *sfname);
extern Task<char *> lookup_file(char *fname, int try_home);

/* epath.c */
extern Task<void> epath_init(void);
extern void cmdstr(int c, char *seq);
extern fn_t getbind(int c);
extern char *getfname(fn_t);
extern fn_t fncmatch(char *);
extern unsigned int stock(char *keyname);
extern char *transbind(char *skey);

/* buffer.c */
extern Task<int> cmd_select_buffer(int f, int n);
extern Task<int> cmd_next_buffer(int f, int n);
extern Task<int> swbuffer(struct buffer *bp);
extern Task<void> shown_buffer_changed(void);
extern Task<int> cmd_delete_buffer(int f, int n);
extern Task<int> destroy_buffer(struct buffer *bp);
extern Task<int> cmd_name_buffer(int f, int n);
extern Task<int> makelist(int iflag);
extern Task<int> cmd_list_buffers(int f, int n);
extern void ltoa(char *buf, int width, long num);
extern int addline(struct buffer *bp, char *text);
extern int any_changed_buffers(void);
extern Task<int> clear_buffer(struct buffer *bp);
extern Task<int> cmd_unmark_buffer(int f, int n);
/* Lookup a buffer by name. */
extern struct buffer *find_buffer(char *bname, int cflag, int bflag);

/* file.c */
extern Task<int> cmd_read_file(int f, int n);
extern Task<int> cmd_insert_file(int f, int n);
extern Task<int> cmd_find_file(int f, int n);
extern Task<int> cmd_view_file(int f, int n);
extern Task<int> getfile(char *fname, int lockfl);
extern Task<int> readin(char *fname, int lockfl);
extern void makename(char *bname, char *fname);
extern void unique_buffer_name(char *name);
extern Task<int> cmd_write_file(int f, int n);
extern Task<int> cmd_save_file(int f, int n);
extern Task<int> writeout(char *fn);
extern Task<int> cmd_change_file_name(int f, int n);
extern Task<int> insert_file(char *fname);
extern Task<int> file_changed(struct buffer *bp, char *fn);

/* fileio.c */
extern Task<int> file_open_read(char *fn);
extern Task<int> file_open_write(char *fn);
extern Task<int> file_close(void);
extern Task<int> file_put_line(char *buf, int nbuf);
extern Task<int> file_get_line(void);
extern Task<int> file_exists(char *fname);

/* exec.c */
extern Task<void> exec_yield(void);
extern Task<int> cmd_execute_named_command(int f, int n);
extern Task<int> cmd_execute_command_line(int f, int n);
extern Task<int> docmd(char *cline);
extern char *token(char *src, char *tok, int size);
extern Task<int> macarg(char *tok);
extern Task<int> nextarg(char *prompt, char *buffer, int size, int terminator);
extern Task<int> cmd_store_macro(int f, int n);
extern Task<int> cmd_store_procedure(int f, int n);
extern Task<int> cmd_execute_procedure(int f, int n);
extern Task<int> cmd_execute_buffer(int f, int n);
extern Task<int> dobuf(struct buffer *bp);
extern void freewhile(struct while_block *wp);
extern Task<int> cmd_execute_file(int f, int n);
extern Task<int> dofile(char *fname);
extern Task<int> execute_numbered_macro(int f, int n, int number);
extern Task<int> cmd_execute_macro_1(int f, int n);
extern Task<int> cmd_execute_macro_2(int f, int n);
extern Task<int> cmd_execute_macro_3(int f, int n);
extern Task<int> cmd_execute_macro_4(int f, int n);
extern Task<int> cmd_execute_macro_5(int f, int n);
extern Task<int> cmd_execute_macro_6(int f, int n);
extern Task<int> cmd_execute_macro_7(int f, int n);
extern Task<int> cmd_execute_macro_8(int f, int n);
extern Task<int> cmd_execute_macro_9(int f, int n);
extern Task<int> cmd_execute_macro_10(int f, int n);
extern Task<int> cmd_execute_macro_11(int f, int n);
extern Task<int> cmd_execute_macro_12(int f, int n);
extern Task<int> cmd_execute_macro_13(int f, int n);
extern Task<int> cmd_execute_macro_14(int f, int n);
extern Task<int> cmd_execute_macro_15(int f, int n);
extern Task<int> cmd_execute_macro_16(int f, int n);
extern Task<int> cmd_execute_macro_17(int f, int n);
extern Task<int> cmd_execute_macro_18(int f, int n);
extern Task<int> cmd_execute_macro_19(int f, int n);
extern Task<int> cmd_execute_macro_20(int f, int n);
extern Task<int> cmd_execute_macro_21(int f, int n);
extern Task<int> cmd_execute_macro_22(int f, int n);
extern Task<int> cmd_execute_macro_23(int f, int n);
extern Task<int> cmd_execute_macro_24(int f, int n);
extern Task<int> cmd_execute_macro_25(int f, int n);
extern Task<int> cmd_execute_macro_26(int f, int n);
extern Task<int> cmd_execute_macro_27(int f, int n);
extern Task<int> cmd_execute_macro_28(int f, int n);
extern Task<int> cmd_execute_macro_29(int f, int n);
extern Task<int> cmd_execute_macro_30(int f, int n);
extern Task<int> cmd_execute_macro_31(int f, int n);
extern Task<int> cmd_execute_macro_32(int f, int n);
extern Task<int> cmd_execute_macro_33(int f, int n);
extern Task<int> cmd_execute_macro_34(int f, int n);
extern Task<int> cmd_execute_macro_35(int f, int n);
extern Task<int> cmd_execute_macro_36(int f, int n);
extern Task<int> cmd_execute_macro_37(int f, int n);
extern Task<int> cmd_execute_macro_38(int f, int n);
extern Task<int> cmd_execute_macro_39(int f, int n);
extern Task<int> cmd_execute_macro_40(int f, int n);

/* spawn.c */
extern Task<int> cmd_interactive_shell(int f, int n);
extern Task<int> cmd_suspend_emacs(int f, int n);
extern Task<void> rtfrmshell(void);
extern Task<int> cmd_shell_command(int f, int n);
extern Task<int> cmd_execute_program(int f, int n);
extern Task<int> cmd_pipe_command(int f, int n);
extern Task<int> cmd_filter_buffer(int f, int n);

/* search.c */
extern Task<int> cmd_search_forward(int f, int n);
extern Task<int> cmd_hunt_forward(int f, int n);
extern Task<int> cmd_search_reverse(int f, int n);
extern Task<int> cmd_hunt_backward(int f, int n);
extern int mcscanner(struct magic *mcpatrn, int direct, int beg_or_end);
extern int scanner(const char *patrn, int direct, int beg_or_end);
extern int eq(unsigned char bc, unsigned char pc);
extern void savematch(void);
extern void rvstrcpy(char *rvstr, char *str);
extern Task<int> cmd_replace_string(int f, int n);
extern Task<int> cmd_query_replace_string(int f, int n);
extern int delins(int dlength, char *instr, int use_meta);
extern int expandp(char *srcstr, char *deststr, int maxlength);
extern int at_buffer_end(struct line *curline, int curoff, int dir);
extern void mcclear(void);
extern void rmcclear(void);

/* isearch.c */
extern Task<int> cmd_reverse_incremental_search(int f, int n);
extern Task<int> cmd_incremental_search(int f, int n);
extern Task<int> isearch(int f, int n);
extern int checknext(char chr, char *patrn, int dir);
extern int scanmore(char *patrn, int dir);
extern int match_pat(char *patrn);
extern int promptpattern(char *prompt);
extern Task<int> get_char(void);

/* eval.c */
extern void varinit(void);
extern Task<char *> eval_function(char *fname);
extern char *user_variable(char *vname);
extern char *environment_variable(char *vname);
extern Task<int> cmd_set(int f, int n);
extern Task<void> findvar(char *var, struct variable_description *vd, int size);
extern Task<int> svar(struct variable_description *var, char *value);
extern char *itoa(int i);
extern int token_type(char *token);
extern Task<char *> getval(char *token, char *result, int size);
extern int truth_value(char *val);
extern char *truth_text(int val);
extern char *mkupper(const char *str, char *result);
extern char *mklower(const char *str, char *result);
extern int abs(int x);
extern int next_random(void);
extern int sindex(char *source, char *pattern);
extern char *xlat(char *source, char *lookup, char *trans);

/* spell.c */
extern void spell_init(void);
extern int spellcheck(const char *word);
