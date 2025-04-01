#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define UTF8_Tail 0b00111111
//NOTE: � is U+FFD -> 0xEF 0xBF 0xBD
//Binary: 11101111 10111111 10111101

#ifdef HAVE_DEBUG
#define dbg(...) printf(__VA_ARGS__)
#define dbg_utf8(...) print_utf8(__VA_ARGS__)
#else
#define dbg(...)
#define dbg_utf8(...)
#endif


typedef struct utf8_string {
    unsigned char* data;
    unsigned int length;
    unsigned int capacity;
} utf8_string;
typedef utf8_string utf8_slice;


int fputs_len(const unsigned char* str, unsigned int len, FILE* stream);
unsigned int decode_utf8_char(unsigned char* Input);
int is_utf8_valid(unsigned char* Input);
int num_byte(unsigned char* Input);
void print_utf8(utf8_string* utf8_str);


utf8_string from(char* input);
void utf8_concat(utf8_string* s1, utf8_string* s2);     //NOTE: Slice volatile
void utf8_concat_literal(utf8_string* s1, char* s2);    //NOTE: Slice volatile
void utf8_push_str_literal(utf8_string* s, char* utf8_char);    //NOTE: Slice volatile
void utf8_iterate_print( utf8_string* s);
void utf8_free(utf8_string* s);
utf8_slice slice_byte(utf8_string* src, unsigned int from, unsigned int till);
utf8_slice slice_char(utf8_string* src, unsigned int from, unsigned int till);
void align_next(utf8_string* src, unsigned int byte_pos);
utf8_string to_owned(utf8_string* slice);
void delete_byte(utf8_string* src, unsigned int from, unsigned int till);
void delete_char(utf8_string* src, unsigned int from, unsigned int till);
void insert (utf8_string* dest, utf8_string* src, unsigned int location);   //NOTE: Slice volatile
int seek_char(utf8_string* src, unsigned int gap);

//NOTE: Handle Overlong Encoding   -- Too man bytes for '/' [X]
//NOTE: Handle Surrogates Pairs    -- UTF-16 Only           [X]
//NOTE: Handle Out of Range        -- Beyond U+10FFFF       [X]
//NOTE: Handle Standalone Byte     -- No lead Byte          [X]
//FIXME: Truncated Sequence        -- Missing Byte          []
//Could use a length perameter for the fix but it seems to lose its modularity.
//When decoding some data sequentially, those functions should handle the problem externally.
// For Truncated Sequence, there will be problem reported by -fsanitize=address.
// The 1st test for Invalid sequnce is where the problem is occurring.

unsigned int decode_utf8_char(unsigned char* Input) {


    unsigned int codepoint = 0;
    int byte = 0;
    // 1-byte (ASCII): 0xxxxxxx
    if (Input[0] < 0b10000000) {
        
        codepoint = Input[0];
        byte = 1;
    }
    // 2-byte sequence: 110xxxxx 10xxxxxx
    else if (Input[0] >= 0b11000010 && Input[0] <= 0b11011111) {
        if ((Input[1] & 0b11000000) != 0b10000000) return 0xFFFD;//NOTE: Invalid Continuetion Byte
        byte = 2;
        codepoint = ((Input[0] & 0b00011111) << 6) | (Input[1] & UTF8_Tail);
    }
    // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
    else if (Input[0] >= 0b11100000 && Input[0] <= 0b11101111) {
        if ((Input[1] & 0b11000000) != 0b10000000 || (Input[2] & 0b11000000) != 0b10000000) return 0xFFFD;//NOTE: Invalid Continuetion Byte
        byte = 3;
        codepoint = ((Input[0] & 0b00001111) << 12) | ((Input[1] & UTF8_Tail) << 6) | (Input[2] & UTF8_Tail);
    }
    // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    // Checking from 0xF0 to 0xF4 range as defined in "UTF-8, a transformation format of ISO 10646"
    else if (Input[0] >= 0b11110000 && Input[0] <= 0b11110100) {
        if ((Input[1] & 0b11000000) != 0b10000000 || (Input[2] & 0b11000000) != 0b10000000 || (Input[3] & 0b11000000) != 0b10000000) return 0xFFFD;//NOTE: Invalid Continuetion Byte
        byte = 4;
        codepoint = ((Input[0] & 0b00000111) << 18) | ((Input[1] & UTF8_Tail) << 12) | ((Input[2] & UTF8_Tail) << 6) | (Input[3] & UTF8_Tail);
    }
    else return 0xFFFD; //NOTE: No lead bytes / Invalid lead bytes.

    //NOTE: Surrogate Pairs from 0xD800 to 0xDFFF
    if (codepoint >= 0b1101100000000000 && codepoint <= 0b1101111111111111) { 
        return 0xFFFD;
    }
    //NOTE: Overlong Encoding
    switch (byte) {
        // Codepoint should not fit in 1 byte (mask: 0b01111111)
        case 2: if ((codepoint & 0b01111111) == codepoint) return 0xFFFD; break;
        // Codepoint should not fit in 2 bytes (mask: 0b0000011111111111)
        case 3: if ((codepoint & 0b0000011111111111) == codepoint) return 0xFFFD; break;
        // Codepoint should not fit in 3 bytes (mask: 0b1111111111111111)
        case 4: if ((codepoint & 0b1111111111111111) == codepoint) return 0xFFFD; break;
    }
    //NOTE: Not checking codepoints beyond U+10FFFF
    if (codepoint > 0x10FFFF) return 0xFFFD;
    return codepoint;
}

int is_utf8_valid(unsigned char* Input){
    if( decode_utf8_char(Input) != 0xFFFD ) return 1;
    else return 0;
}
//FIXME: Wierd function
int num_byte(unsigned char* Input){

    unsigned int codepoint = 0;
    int byte = 0;
    // 1-byte (ASCII): 0xxxxxxx
    if (Input[0] < 0b10000000) {
        codepoint = Input[0];
        byte = 1;
    }
    // 2-byte sequence: 110xxxxx 10xxxxxx
    else if (Input[0] >= 0b11000010 && Input[0] <= 0b11011111) {
        if ((Input[1] & 0b11000000) != 0b10000000) return 0xFFFD;//NOTE: Invalid Continuetion Byte
        byte = 2;
        codepoint = ((Input[0] & 0b00011111) << 6) | (Input[1] & UTF8_Tail);
    }
    // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
    else if (Input[0] >= 0b11100000 && Input[0] <= 0b11101111) {
        if ((Input[1] & 0b11000000) != 0b10000000 || (Input[2] & 0b11000000) != 0b10000000) return 0xFFFD;//NOTE: Invalid Continuetion Byte
        byte = 3;
        codepoint = ((Input[0] & 0b00001111) << 12) | ((Input[1] & UTF8_Tail) << 6) | (Input[2] & UTF8_Tail);
    }
    // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    // Checking from 0xF0 to 0xF4 range as defined in "UTF-8, a transformation format of ISO 10646"
    else if (Input[0] >= 0b11110000 && Input[0] <= 0b11110100) {
        if ((Input[1] & 0b11000000) != 0b10000000 || (Input[2] & 0b11000000) != 0b10000000 || (Input[3] & 0b11000000) != 0b10000000) return 0xFFFD;//NOTE: Invalid Continuetion Byte
        byte = 4;
        codepoint = ((Input[0] & 0b00000111) << 18) | ((Input[1] & UTF8_Tail) << 12) | ((Input[2] & UTF8_Tail) << 6) | (Input[3] & UTF8_Tail);
    }
    else return 0xFFFD; //NOTE: No lead bytes / Invalid lead bytes.

    //NOTE: Surrogate Pairs from 0xD800 to 0xDFFF
    if (codepoint >= 0b1101100000000000 && codepoint <= 0b1101111111111111) { 
        return 0xFFFD;
    }
    //NOTE: Overlong Encoding
    switch (byte) {
        // Codepoint should not fit in 1 byte (mask: 0b01111111)
        case 2: if ((codepoint & 0b01111111) == codepoint) return 0xFFFD; break;
        // Codepoint should not fit in 2 bytes (mask: 0b0000011111111111)
        case 3: if ((codepoint & 0b0000011111111111) == codepoint) return 0xFFFD; break;
        // Codepoint should not fit in 3 bytes (mask: 0b1111111111111111)
        case 4: if ((codepoint & 0b1111111111111111) == codepoint) return 0xFFFD; break;
    }
    //NOTE: Not checking codepoints beyond U+10FFFF
    if (codepoint > 0x10FFFF) return 0xFFFD;
    return byte;
}


//NOTE: print_utf8 had to be implemented in much more detail
//using graphics rendaring or other libraries.
//What fputs is doing here:
//  -> Decode
//  -> Find character
//  -> Genarate
//  -> Show to stdout
//TODO: A printf like functionality would we nice addition.
void print_utf8(utf8_string* utf8_str) {
    fputs_len( utf8_str->data, utf8_str->length, stdout);
}
//FIXME: Incompete implementaiton
//FIXME: dbg_utf8 function is appropriate for printing UTF-8 strings
//in environments where the ->locale<- and ->terminal<- support UTF-8 encoding.
//Always ensure that your environment is correctly configured to handle 
//UTF-8 to avoid issues with character display and encoding.
int fputs_len(const unsigned char* str, unsigned int len, FILE* stream) {
    // Write the string up to the specified length
    for (unsigned int i = 0; i < len; i++) {
        if (fputc(str[i], stream) == EOF) {
            return EOF;  // Return EOF if an error occurs
        }
    }
    return len;  // Return the number of characters written
}

//NOTE: Failed interop avoided because string is taken from the first NULL byte.
// Using NULL bytes to break the system should not be possible.
//FIXME: NULL is INVALID []
utf8_string from(char* input) {

    int len = strlen(input);    //works for string literal
    dbg("input size -> %u\n", len);
    utf8_string s;
    s.data = (unsigned char*)malloc(len);
    memcpy(s.data, input, len);
    s.length = len; // Wont keep NULL
    s.capacity = len;
    dbg("s.data     -> ");
    dbg_utf8(&s);
    dbg("\n");
    dbg("s.length   -> %u\n", s.length);
    dbg("s.capacity -> %u\n", s.capacity);
    return s;
}


void utf8_concat(utf8_string* s1, utf8_string* s2) {
    //NOTE: Handles Invalid Slice writes
    if(s1->capacity == 0){
            fprintf(stderr, "Invalid write through slice\n");
    }
    int new_size = s1->length + s2->length;

    if( new_size > s1->capacity){
        unsigned int new_cap = new_size * 3/2 ;    //NOTE: Increase by 1.5x for geometric growth
        unsigned char* new_data = (unsigned char*)realloc(s1->data, new_size);         
        if (!new_data){ //NOTE: Handle realloc failure
            fprintf(stderr, "Memory allocation failed\n");
            return;
        }
        s1->data = new_data;
        s1->capacity = new_cap;
        //FIXME: Possible free pointer use may occur after reallocaion through slice access.
    }

    memcpy(s1->data + s1->length, s2->data, s2->length);
    s1->length = s1->length + s2->length;
    //NOTE:Avoid using s1->length = strlen(data) because,
    //uneven capacity and length strings might cause prooblem
    dbg("s1.data     -> ");
    dbg_utf8(s1);
    dbg("\n");
    dbg("s1.length   -> %u\n", s1->length);
    dbg("s1.capacity -> %u\n", s1->capacity);
} 

void utf8_concat_literal(utf8_string* s1, char* s2) {
    //NOTE: Handles Invalid Slice writes
    if(s1->capacity == 0){
            fprintf(stderr, "Invalid write through slice. capacity : %u\n", s1->capacity);
    }
    int len2 = strlen(s2);
    int new_size = s1->length + len2; 

    if( new_size > s1->capacity){
        s1->capacity = new_size;
        s1->data = (unsigned char*)realloc(s1->data, new_size);
    }

    memcpy(s1->data + s1->length, s2, len2);
    s1->length = s1->length + len2;
    //NOTE:Avoid using s1->length = strlen(data) because,
    //uneven capacity and length strings might cause prooblem
    dbg("s1.data     -> ");
    dbg_utf8(s1);
    dbg("\n");
    dbg("s1.length   -> %u\n", s1->length);
    dbg("s1.capacity -> %u\n", s1->capacity);
}

void utf8_push_str_literal(utf8_string* s, char* utf8_char) {
    utf8_concat_literal(s, utf8_char);
}

void utf8_free(utf8_string* s) {
    if(s->capacity == 0){   //NOTE: No risk of double free.
        s->data = NULL;
        s->length = 0;
        //        printf("utf8_free : Error: Slice free attempt\n");
        return;
    }
    free(s->data);
    s->data = NULL;
    s->length = 0;
    s->capacity = 0;
}

void utf8_iterate_print( utf8_string* s){

    for( int i=0; i<s->length; i++){
        //FIXME: Incomplete implementation
        //Used fputc instead of print_utr8
        //        fputc(s->data[i], stdout);
        printf("%x ", s->data[i]);
    }
    printf("\n");
}

utf8_slice slice_byte(utf8_string* src, unsigned int from, unsigned int till){
    utf8_string slice = *src;
    slice.capacity = 0;
    //NOTE: Buffer overflow / Invalid Memory Access Checked
    if( till  > src->length ){
        printf("slice_byte : Error: Bufferflow\n");
        slice.data  = NULL;
        slice.length   = 0;
        return slice;
    }
    slice.data += from;
    //NOTE: Corrected Indexing. Now slice_byte(src, 0, 4) will have a length of 5
    slice.length = till - from + 1;
    return slice;
}

utf8_slice slice_char(utf8_string* src, unsigned int from, unsigned int till){
    //NOTE: Object Out of Bound
    if( till > src->length || from > src->length || (till - from) > src->length ){ 
    dbg("till > length          -> %d\n", till >src->length);
    dbg("from > length          -> %d\n", from >src->length);
    dbg("(till - from) >length  -> %d\n", (till-from) > src->length);
        utf8_slice slice = { 
        .data = NULL,
        .length = 0,
        .capacity = 0
    };
        return slice;
    }

    //TODO: Memoization search system could be applied here.
    //NOTE: Auto security checks in seek_char.
    unsigned int char_till = seek_char(src, till);
    unsigned int char_from = seek_char(src, from);
    utf8_string slice = slice_byte(src, char_from, char_till);
    if( slice.length > 0) align_next(&slice, slice.length);
    dbg("slice.data     -> \n");
    dbg_utf8(&slice);
    dbg("\nslice.length   -> %u\n", slice.length);
    dbg("slice.capacity -> %u\n", slice.capacity);
    return slice;
}

void align_next(utf8_string* src, unsigned int byte_pos) {
    
    if      (is_utf8_valid(&src->data[byte_pos])  &&  (byte_pos >= src->length))  return; // ASCII (0xxxxxxx)
    else if (is_utf8_valid(&src->data[byte_pos+1]) && ((byte_pos+1) >= src->length)) src->length++;// 2-byte UTF-8 (110xxxxx)
    else if (is_utf8_valid(&src->data[byte_pos+2]) && ((byte_pos+2) >= src->length)) src->length += 2;// 3-byte UTF-8 (1110xxxx)
    else if (is_utf8_valid(&src->data[byte_pos+3]) && ((byte_pos+3) >= src->length)) src->length += 3;// 4-byte UTF-8 (11110xxx)
    fprintf(stderr, "Out of bounds byte position\n");
}


//TODO: to_owned.slice  [X]
//TODO: first_match     []
//TODO: seek_char       [X]
//TODO: delete_byte     [X]
//TODO: delete_char     [X]
//TODO: insert          [X]
//TODO: slice_char      [X]

utf8_string to_owned(utf8_string* slice){
    utf8_string string;
    string.data = (unsigned char*)malloc(slice->length); 
    //FIXME: Handle Allocation failure
    memcpy(string.data, slice->data, slice->length);
    string.length = slice->length;
    string.capacity = slice->length;
    return string;
}

int seek_char(utf8_string* src, unsigned int gap){
    //NOTE: NULL input and NULL data check
    if( !src || !src->data || !gap) return 0;

    dbg("src->length : %u\n", src->length);
    int seek = 0;
    for(int i=0;i<gap;i++){ 

        if( seek >= src->length) return -1;  //NOTE: Object out of bound. Can be used to count how many UTF-8 character is in a utf8_string.
        int bytes = num_byte(&src->data[seek]);
        if (bytes == 0xFFFD){   // num_byte returns 0xFFFD on Error;
            fprintf(stderr, "Invalid UTF-8 encoding encountered\n"); 
            //NOTE: Wondering if I should use this on other functions
            return -1;
        }
        seek += bytes;
        if (seek > src->length) {
            fprintf(stderr, "Truncated UTF-8 sequence\n"); //FIXME: Fixed the issue partially with decode_utf8_char function. A Truncation in the middle of the sequence should still be a problem.
            return -1;
        }
    }
    dbg("Seek: %d\n", seek);
    return seek;
}
void delete_byte(utf8_string* src, unsigned int from, unsigned int till){
 
    //NOTE: Object Out of Bound
    if( till > src->length || from > src->length || (till - from) > src->length ){ 
    fprintf(stderr, "Out of bounds byte position\n");
    return;
    }
    utf8_slice slice_2 = slice_byte(src, till + 1,src->length); //NOTE: till + 1 for the next byte from till(th) byte
    memmove(src->data + from, slice_2.data, src->length - till);
    src->length = src->length - (till - from + 1); //NOTE: FIXED length indexing
}

void delete_char(utf8_string* src, unsigned int from, unsigned int till){
    int char_count = 0;
    for(int i=0; i<src->length; i++){
        if(is_utf8_valid(&src->data[i])) char_count++;
    }
    //NOTE: Object Out of Bound
    if( till > char_count || from > char_count || (till - from) > char_count ){ 
        fprintf(stderr, "Out of bounds byte position\n");
        return;
    }
 
    int pos_head = seek_char(src, from);
    int pos_tail = seek_char(src, till + 1); //NOTE: The next character from till
    

    utf8_slice slice_2 = slice_byte(src, pos_tail, src->length);
    memcpy(src->data + pos_head, slice_2.data, src->length - pos_tail);
    dbg("deleted_char.data     -> ");
    dbg_utf8(src);
    dbg("\n");
    dbg("deleted_char.length   -> %u\n", src->length);
    dbg("deleted_char.capacity -> %u\n", src->capacity);

    src->length = src->length - (pos_tail - pos_head);

}

void insert (utf8_string* dest, utf8_string* src, unsigned int location){
    //NOTE: Handles Invalid Slice writes
    if(dest->capacity == 0){
            fprintf(stderr, "Invalid write through slice\n");
    }
    //NOTE: Here string slices can be inserted. But, can't be inserted to a slice.


    int new_size = dest->length + src->length;

    if( new_size > dest->capacity){
        dest->capacity = new_size;    //TODO: Increase by 1.5x for performance
        dest->data = (unsigned char*)realloc(dest->data, new_size); //TODO: Handle realloc failure
        //FIXME: Possible free pointer use may occur after reallocaion through slice access.
    }
    unsigned int pos_insert = seek_char(dest, location);
    // Shift existing data to make space for new insertion
    memmove(dest->data + pos_insert + src->length, dest->data + pos_insert, dest->length - pos_insert);
    // Insert new string
    memcpy(dest->data + pos_insert, src->data, src->length);
    // Update length
    dest->length += src->length;
    //NOTE:Avoid using s1->length = strlen(data) because,
    //uneven capacity and length strings might cause prooblem
    dbg("dest.data     -> ");
    dbg_utf8(dest);
    dbg("\n");
    dbg("dest.length   -> %u\n", dest->length);
    dbg("dest.capacity -> %u\n", dest->capacity);


}
/*
int main() {
    utf8_string pera_1 = from("ٱلسَّلَامُ عَلَيْكُمْ\n");
    utf8_slice  slice_1 = slice_char(&pera_1, 0, 9);
    int seek            = seek_char(&slice_1, 18);
    utf8_slice slice_2  = slice_byte(&slice_1, 3, 15);
    utf8_slice slice_3  = slice_byte(&slice_1, 3, 15);
    utf8_slice slice_4  = slice_byte(&slice_1, 3, 15);
    utf8_slice slice_5  = slice_byte(&slice_1, 3, 15);
    print_utf8(&pera_1);
    printf("Byte Iteration pera_1   : ");
    utf8_iterate_print(&pera_1);
    printf("\n");
    print_utf8(&slice_1);
    printf("\n");
    align_head_next(&slice_2);
    print_utf8(&slice_2);
    printf("\n");
    align_head_priv(&slice_3);
    print_utf8(&slice_3);
    printf("\n");
    align_tail_next(&slice_4);
    print_utf8(&slice_4);
    printf("\n");
    align_head_priv(&slice_5);
    print_utf8(&slice_5);
    printf("\n");





    return 0;
}
*/
