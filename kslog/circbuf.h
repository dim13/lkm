/*
 * circbuf.h
 *
 * circular buffer interface
 *
 * mike@gravitino.net
 */
 
typedef struct circular_buffer
{
	int len ;
	int size;
	int next;
	int curr;
	int loop;

	unsigned char *buf;

} circular_buffer;

/*
 * initialize circular_buffer structure:
 * zero out structure & save buf & len args to structure members
 */
void	cb_init(circular_buffer *cb, unsigned char *buf, int len);

/*
 * place character into circular buffer
 */
void	cb_putc(circular_buffer *cb, char ch);

/*
 * remove character from circular buffer
 */
int	cb_getc(circular_buffer *cb, char *ch);
