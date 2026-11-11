#ifdef __ANDROID__
/* Forward exit notification text to GameActivity. */
void bye_notice_show(const char *text) { (void)kb_call_text_method("showByeNotice", text); }
#else
void bye_notice_show(const char *text) { (void)text; }
#endif
