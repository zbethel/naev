/*
 * See Licensing and Copyright notice in naev.h
 */

/**
 * @file nlua_economy.c
 *
 * @brief Lua economy module.
 */

#include "nlua_news.h"

#include "naev.h"
#include "land.h"

#include <lauxlib.h>

#include "nlua.h"
#include "nluadef.h"
#include <string.h>

extern news_t* news_list;   /**< Linked list containing all articles */

int newsL_addArticle( lua_State *L );
int newsL_freeArticle( lua_State *L );
int newsL_get( lua_State *L );
Lua_article* luaL_validarticle( lua_State *L, int ind );
int lua_isarticle( lua_State *L, int ind );
Lua_article* lua_pusharticle( lua_State *L, Lua_article article );
int newsL_eq( lua_State *L );
int newsL_title( lua_State *L );
int newsL_desc( lua_State *L );
int newsL_faction( lua_State *L );
int newsL_date( lua_State *L );

static const luaL_reg economy_methods[] = {
   {"add",newsL_addArticle},
   {"rm",newsL_freeArticle},
   {"get",newsL_get},
   {"title",newsL_title},
   {"desc",newsL_desc},
   {"faction",newsL_faction},
   {"date",newsL_date},
   {"__eq",newsL_eq},
   {0,0}
}; /**< System metatable methods. */
static const luaL_reg economy_cond_methods[] = {
   {"get",newsL_get},
   {"title",newsL_title},
   {"desc",newsL_desc},
   {"faction",newsL_faction},
   {"date",newsL_date},
   {"__eq",newsL_eq},
   {0,0}
}; /**< Read only economy metatable methods. */

/**
 * @brief Loads the economy library.
 *
 *    @param L State to load economy library into.
 *    @param readonly Load read only functions?
 *    @return 0 on success.
 */
int nlua_loadNews( lua_State *L, int readonly )
{

   /* Create the metatable */
   luaL_newmetatable(L, ARTICLE_METATABLE);

   /* Create the access table */
   lua_pushvalue(L,-1);
   lua_setfield(L,-2,"__index");

   /* Register the values */
   if (readonly)
      luaL_register(L, NULL, economy_cond_methods);
   else
      luaL_register(L, NULL, economy_methods);

   /* Clean up. */
   lua_setfield(L, LUA_GLOBALSINDEX, ARTICLE_METATABLE);

   return 0; /* No error */
}

/**
 * @brief Adds an article
 *
 * @usage s = news.add( "Empire", "Hello world!", "The Empire wishes to say hello!", 0 ) -- Adds a generic news item.
 *
 *    @luaparam faction faction of the article, "Generic" for non-factional
 *    @luaparam title Title of the article
 *    @luaparam content What's in the article
 *    @luaparam date What time to put, use 0 to not use a date
 *    @luareturn The article matching name or nil if error.
 * @luafunc get( s )
 */
int newsL_addArticle( lua_State *L ){

   printf("\nAdding new Lua article");

   news_t* n_article;
   Lua_article Larticle;
   char* title;
   char* content;
   char* faction;
   ntime_t date;

   if (lua_isstring(L,1) && lua_isstring(L,2) && lua_isstring(L,3) && lua_isnumber(L,4)){
      faction=strdup(lua_tostring(L,1));
      title=strdup(lua_tostring(L,2));
      content=strdup(lua_tostring(L,3));
      date = (ntime_t) lua_tonumber(L,4);
   }
   else{
      WARN("\nBad arguments, use addArticle(\"Title\",\"Content\",\"Faction\",date)");
      return 0;
   }

   if (title && content && faction)
      n_article = new_article(title, content, faction, date);


   Larticle.id = n_article->id;
   lua_pusharticle(L, Larticle);

   free(title);
   free(content);
   free(faction);


   return 1;
}

/**
 * @brief frees an article
 *    @luaparam Lua_article article to free
 */
int newsL_freeArticle( lua_State *L ){

   Lua_article* Larticle;

   Larticle = luaL_validarticle(L,1);

   free_article(Larticle->id);

   return 1;
}

/**
 * @brief gets all matching articles in a table
 *    @luaparam characteristic characteristic to match, boolean for all
 *    @luareturn a table with matching factions
 */
int newsL_get( lua_State *L )
{
   Lua_article Larticle;
   ntime_t date=-1;
   news_t* article_ptr = news_list;
   char* characteristic=NULL;
   int k;
   int print_all=0;

   if (lua_isnumber(L,1))
      date=(ntime_t) lua_tonumber(L,1);
   else if (lua_isstring(L,1))
      characteristic = strdup(lua_tostring(L,1));
   else if (lua_isboolean(L,1)){
      print_all=1;
   }

         /* Now put all the matching articles in a table. */
   lua_newtable(L);
   k = 1;
   do {

      if (article_ptr->title==NULL || article_ptr->desc == NULL 
         || article_ptr->faction == NULL  )
         continue;

      if (print_all || date==article_ptr->date || (characteristic && (!strcmp(article_ptr->title,characteristic) || 
         !strcmp(article_ptr->desc,characteristic) || !strcmp(article_ptr->faction,characteristic))) )
      {
         lua_pushnumber(L, k++); /* key */
         Larticle.id = article_ptr->id;
         lua_pusharticle(L, Larticle); /* value */
         lua_rawset(L,-3); /* table[key] = value */
      }

   }while( (article_ptr = article_ptr->next) != NULL );

   free(characteristic);

   return 1;

}


/**
 * @brief Check articles for equality.
 *
 * Allows you to use the '=' operator in Lua with articles.
 *
 *    @luaparam a1 article 1
 *    @luaparam a2 article 2
 *    @luareturn true if both systems are the same.
 * @luafunc __eq( s, comp )
 */
int newsL_eq( lua_State *L )
{
   Lua_article *a1;
   Lua_article *a2;
   a1 = luaL_validarticle( L,1 );
   a2 = luaL_validarticle( L,2 );
   if (a1->id == a2->id)
      lua_pushboolean(L,1);
   else
      lua_pushboolean(L,0);
   return 1;
}

/**
 * @brief Makes sure the article is valid or raises a Lua error.
 *
 *    @param L State currently running.
 *    @param ind Index of the article to validate.
 *    @return The article (doesn't return if fails - raises Lua error ).
 */
Lua_article* luaL_validarticle( lua_State *L, int ind )
{

   if (lua_isarticle(L, ind)) {
      return (Lua_article*) lua_touserdata(L,ind);
   }
   else {
      luaL_typerror(L, ind, ARTICLE_METATABLE);
      return NULL;
   }

   NLUA_ERROR(L, "article is invalid.");

   return NULL;
}

/**
 * @brief Checks to see if ind is an article
 *
 *    @param L Lua state to check.
 *    @param ind Index position to check.
 *    @return 1 if ind is a article.
 */
int lua_isarticle( lua_State *L, int ind )
{
   int ret;

   if (lua_getmetatable(L,ind)==0)
      return 0;
   lua_getfield(L, LUA_REGISTRYINDEX, ARTICLE_METATABLE);

   ret = 0;
   if (lua_rawequal(L, -1, -2))  /* does it have the correct mt? */
      ret = 1;

   lua_pop(L, 2);  /* remove both metatables */
   return ret;
}


/**
 * @brief Pushes an article on the stack.
 *
 *    @param L Lua state to push article into.
 *    @param article article to push.
 *    @return Newly pushed article.
 */
Lua_article* lua_pusharticle( lua_State *L, Lua_article article )
{
   Lua_article *o;
   o = (Lua_article*) lua_newuserdata(L, sizeof(Lua_article));
   *o = article;
   luaL_getmetatable(L, ARTICLE_METATABLE);
   lua_setmetatable(L, -2);
   return o;
}




/**
 * @brief gets the article title
 *    @luaparam a article to get the title of
 *    @luareturn title
 * @luafunc news.title(a)
 */
int newsL_title( lua_State *L )
{
   Lua_article *a;
   news_t* article_ptr;
   if (!(a = luaL_validarticle( L,1 ))){
      WARN("Bad argument to news.date(), must be article");
      return 0;}
   if ((article_ptr=news_get(a->id))==NULL){
      WARN("\nArticle not valid");
      return 0;
   }
   lua_pushstring(L,article_ptr->title);
   return 1;
}

/**
 * @brief gets the article desc
 *    @luaparam a article to get the desc of
 *    @luareturn desc
 * @luafunc news.desc(a)
 */
int newsL_desc( lua_State *L )
{
   Lua_article *a;
   news_t* article_ptr;
   if (!(a = luaL_validarticle( L,1 ))){
      WARN("Bad argument to news.date(), must be article");
      return 0;}
   if ((article_ptr=news_get(a->id))==NULL){
      WARN("\nArticle not valid");
      return 0;
   }
   lua_pushstring(L,article_ptr->desc);
   return 1;
}

/**
 * @brief gets the article faction
 *    @luaparam a article to get the faction of
 *    @luareturn faction
 * @luafunc news.faction(a)
 */
int newsL_faction( lua_State *L )
{
   Lua_article *a;
   news_t* article_ptr;
   if (!(a = luaL_validarticle( L,1 ))){
      WARN("Bad argument to news.date(), must be article");
      return 0;}
   if ((article_ptr=news_get(a->id))==NULL){
      WARN("\nArticle not valid");
      return 0;
   }
   lua_pushstring(L,article_ptr->faction);
   return 1;
}

/**
 * @brief gets the article date
 *    @luaparam a article to get the date of
 *    @luareturn date
 * @luafunc news.date(a)
 */
int newsL_date( lua_State *L )
{
   Lua_article *a;
   news_t* article_ptr;
   if (!(a = luaL_validarticle( L,1 ))){
      WARN("Bad argument to news.date(), must be article");
      return 0;}
   if ((article_ptr=news_get(a->id))==NULL){
      WARN("\nArticle not valid");
      return 0;
   }
   lua_pushinteger(L,(lua_Integer)article_ptr->date);
   return 1;
}