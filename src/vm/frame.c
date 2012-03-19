#include "frame.h"
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"

static struct hash frames;
static struct lock frames_lock;
static

void lock_frames (void);
void unlock_frames (void);
bool frame_less (const struct hash_elem *, const struct hash_elem *, void *);
unsigned frame_hash (const struct hash_elem *, void *);


void
lock_frames (){
	lock_acquire (&frames_lock);
}

void unlock_frames (){
	lock_release (&frames_lock);
}

void
frame_init (){
	hash_init (&frames, frame_hash, frame_less, NULL);
	lock_init (&frames_lock);
}

void *
frame_get (void * upage, bool zero){
	void * kpage = palloc_get_page ( PAL_USER | (zero ? PAL_ZERO : 0) );

	/* There is no more free memory, we need to free some */
	if( kpage == NULL ) {
		kpage = evict( upage, thread_current() );
	}

	/* We succesfully allocated space for the page */
	if( kpage != NULL ){
		struct frame * frame = (struct frame*) malloc (sizeof (struct frame));
		frame -> addr = kpage;
		frame -> upage = upage;
		frame -> thread = thread_current ();

		lock_frames();
		hash_insert (&frames, &frame -> hash_elem);
		unlock_frames();
	}

	return kpage;
}

bool
frame_free (void * addr){
	struct frame * frame;
	struct hash_elem * found_frame;
	struct frame frame_elem;
	frame_elem.addr = addr;

	found_frame = hash_find(&frames, &frame_elem.hash_elem);
	if( found_frame != NULL ){
		frame = hash_entry (found_frame, struct frame, hash_elem);

		lock_frames();
		palloc_free_page (frame->addr);
		hash_delete ( &frames, &frame->hash_elem );
		free (frame->addr);
		unlock_frames();


		return true;
	} else {
		return false;
		//Wellll... Nothing to do here :)
	}
}

struct frame *
frame_find (void * addr){
	struct frame * frame;
	struct hash_elem * found_frame;
	struct frame frame_elem;
	frame_elem.addr = addr;

	found_frame = hash_find (&frames, &frame_elem.hash_elem);
	if( found_frame != NULL ){
		//Not locked for better performance
		frame = hash_entry (found_frame, struct frame, hash_elem);
		return frame;
	} else {
		return NULL;
	}
}

bool
frame_less (const struct hash_elem *a_, const struct hash_elem *b_	, void *aux UNUSED){
	const struct frame * a = hash_entry (a_, struct frame, hash_elem);
	const struct frame * b = hash_entry (b_, struct frame, hash_elem);
	return a->addr < b->addr;
}

unsigned
frame_hash(const struct hash_elem *fe, void *aux UNUSED){
	const struct frame * frame = hash_entry (fe, struct frame, hash_elem);
	return hash_int ((unsigned)frame->addr); //Dirty conversion
}

int get_class( uint32_t * , const void * );

int
get_class( uint32_t * pd, const void * page ){
	bool dirty = pagedir_is_dirty ( pd, page );
	bool accessed = pagedir_is_accessed ( pd, page );

	return (accessed) ? (( dirty ) ? 4 : 2) : (( dirty ) ? 3 : 1);
}

void
page_dump( uint32_t * pd, void * page, struct frame * frame ){
	/*bool dirty = pagedir_is_dirty ( pd, page );

	if( dirty ){
		if( write_buffer -> isFull() ){
			PANIC("WTF HAS JUST HAPPENED?!");
			//write_buffer -> forceWrite();
		}

		write_buffer -> add( pd, page );
	}

	pagedir_set_accessed ( pd, page, false );
	pagedir_set_dirty ( pd, page, false );*/
}

void *
evict( void * upage, struct thread * th ){
	struct hash_iterator it;
	uint32_t * pd = th->pagedir;
	void * kpage = NULL;
	int i;

	for( i = 0; i < 2 && kpage == NULL; i++ ){
		hash_first (&it, &frames);
		while(kpage == NULL && hash_next (&it)){
			struct frame *f = hash_entry (hash_cur (&it), struct frame, hash_elem);
			if( f->thread == th ){
				int class = get_class (pd, upage);
				if( class == 1 ){
					page_dump (pd, upage, f);
					kpage = f->addr;
				}
			}
		}

		hash_first (&it, &frames);
		while(kpage == NULL && hash_next (&it)){
			struct frame *f = hash_entry (hash_cur (&it), struct frame, hash_elem);
			if( f->thread == th ){
				int class = get_class (pd, upage);
				if( class == 3 ){
					page_dump (pd, upage, f);
					kpage = f->addr;
				}
				pagedir_set_accessed (pd, upage, false);
			}
		}
	}

	return kpage;
}