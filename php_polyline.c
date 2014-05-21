#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_math.h"
#include "php_polyline.h"

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    PHP_INI_ENTRY("polyline.tuple","2",PHP_INI_ALL,NULL) // Change to enforce short
    PHP_INI_ENTRY("polyline.precision","5",PHP_INI_ALL,NULL)
PHP_INI_END()
/* }}} */

ZEND_BEGIN_ARG_INFO(arginfo_polyline_encode,0)
    ZEND_ARG_INFO(0,array)
ZEND_END_ARG_INFO();
ZEND_BEGIN_ARG_INFO(argingo_polyline_decode,0)
    ZEND_ARG_INFO(0,string)
ZEND_END_ARG_INFO();

/* {{{ polyline_functions[]
 */
zend_function_entry polyline_functions[] = {
    PHP_FE(polyline_encode,arginfo_polyline_encode)
    PHP_FE(polyline_decode,argingo_polyline_decode)
    {NULL,NULL,NULL}
};
/* }}} */

/* {{{ polyline_module_entry
 */
zend_module_entry polyline_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_POLYLINE_EXTNAME,/*Name*/
    polyline_functions, /*Function*/
    PHP_MINIT(polyline), /*Init*/
    PHP_MSHUTDOWN(polyline), /*shutdown*/
    NULL, /*User init*/
    NULL, /*User shutdown*/
    PHP_MINFO(polyline), /*Info*/
    PHP_POLYLINE_VERSION, /*Version*/
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_POLYLINE
ZEND_GET_MODULE(polyline)
#endif

PHP_MINIT_FUNCTION(polyline)
{
    REGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(polyline)
{
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_MINFO_FUNCTION(polyline)
{
    php_info_print_table_start();
    php_info_print_table_header(2,"polyline support","enabled");
    php_info_print_table_row(2,"author","E. McConville <emcconville@emcconville.com>");
    php_info_print_table_row(2,"version",PHP_POLYLINE_VERSION);
    php_info_print_table_row(2,"polyline.tuple",INI_ORIG_STR("polyline.tuple"));
    php_info_print_table_row(2,"polyline.precision",INI_STR("polyline.precision"));
//    DISPLAY_INI_ENTRIES();
    php_info_print_table_end();
}

PHP_FUNCTION(polyline_encode)
{
    zval *zpoint, **data, **point;
    HashTable *zpoint_hash, *point_hash;
    HashPosition pointer;
    smart_str encoded = {0};
    int precision = INI_INT("polyline.precision");
    if( precision < 1 ) precision = 1;
    if( precision > 6 ) precision = 6;
    int tuple = INI_INT("polyline.tuple");
    if( tuple < 1 ) tuple = 1;
    int tuple_index = 0;
    int * pLatLng = ecalloc(sizeof(int), tuple);
    int * cLatLng = ecalloc(sizeof(int), tuple);
    int * delta   = ecalloc(sizeof(int), tuple);
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &zpoint) == FAILURE) {
       return;
    }
    zpoint_hash = Z_ARRVAL_P(zpoint);
    for(
        zend_hash_internal_pointer_reset_ex(zpoint_hash,&pointer);
        zend_hash_get_current_data_ex(zpoint_hash,(void **)&data, &pointer) == SUCCESS;
        zend_hash_move_forward_ex(zpoint_hash,&pointer)
    ){
        // **data is the corrent zval
        if(Z_TYPE_PP(data) == IS_STRING) {
            smart_str_appendl(&encoded, Z_STRVAL_PP(data), Z_STRLEN_PP(data));
        }
        if(Z_TYPE_PP(data) == IS_ARRAY) {
            point_hash = Z_ARRVAL_PP(data);
            // Iterate over nexted point
            
            if(zend_hash_index_exists(point_hash,tuple-1))
            {
                while(tuple_index < tuple)
                {
                
                   zend_hash_index_find(point_hash,tuple_index,(void **)&point);
                   cLatLng[tuple_index] = (int)(Z_DVAL_PP(point) * pow(10,precision));
                   delta[tuple_index] = cLatLng[tuple_index] - pLatLng[tuple_index];
                   pLatLng[tuple_index] = cLatLng[tuple_index];
                   _polyline_encode_chunk(delta[tuple_index],&encoded);
                   tuple_index++; 
               }
               tuple_index = 0;
            } else {
                php_printf("No tuple\n");
            }
        } else {
            php_printf("Not array");
        }
    }
    efree(pLatLng);
    efree(cLatLng);
    efree(delta);
    smart_str_0(&encoded);
    ZVAL_STRINGL(return_value,encoded.c,encoded.len,1);
    smart_str_free(&encoded);
}

PHP_FUNCTION(polyline_decode)
{
    char *encoded;
    int len,index = 0;
    zval *zpoint = NULL;
    int precision = INI_INT("polyline.precision");
    if( precision < 1 ) precision = 1;
    if( precision > 6 ) precision = 6;
    int tuple = INI_INT("polyline.tuple");
    if( tuple < 1 ) tuple = 1;
    int tuple_index = 0;
    int * pLatLng = ecalloc(sizeof(int), tuple);
    int * cLatLng = ecalloc(sizeof(int), tuple);
    int * delta   = ecalloc(sizeof(int), tuple);
    if(zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &encoded, &len) == FAILURE) {
        return;
    }
//    ALLOC_INIT_ZVAL(zpoints);
    array_init(return_value);
    while( index < len)
    {
        delta[tuple_index] = _polyline_decode_chunk( encoded, &index );
        cLatLng[tuple_index] = pLatLng[tuple_index] + delta[tuple_index];
        pLatLng[tuple_index] = cLatLng[tuple_index];
        if(!zpoint) {
            MAKE_STD_ZVAL(zpoint);
            array_init_size(zpoint,tuple);
        }
        add_next_index_double(zpoint,((double)cLatLng[tuple_index] * 1 / pow(10,precision) ));
        tuple_index++;
        // Complte tuple, allocated array & rest
        if(tuple_index == tuple)
        {
            add_next_index_zval(return_value,zpoint);
            zpoint = NULL;
            tuple_index = 0;
        }
    }
    efree(zpoint); 
    efree(pLatLng);
    efree(cLatLng);
    efree(delta);
    efree(encoded);
}


void _polyline_encode_chunk( long delta, smart_str * buffer )
{
    long number = (delta < 0) ? ~(delta << 1) : (delta << 1);
    while ( number >= 0x20 )
    {
        smart_str_appendc(buffer,(char)(0x20 | (number & 0x1f)) + 0x3f);
        number >>= 0x05;
    }
    smart_str_appendc(buffer,(char)number + 0x3f);
}

long _polyline_decode_chunk( char * buffer, int * buffer_length )
{
    long chunk, shift, result;
    char c;
    shift = result = 0;
    do
    {
        c = buffer[(*buffer_length)++];
        chunk = (long)c - 0x3f;
        result |= ( chunk & 0x1f ) << shift;
        shift  += 0x05;
    } while ( chunk >= 0x20 );
    return ( result & 1 ) ? ~( result >> 1 ) : ( result >> 1 );
}