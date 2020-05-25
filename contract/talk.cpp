#include <eosio/eosio.hpp>

// Message table
struct [[eosio::table("message"), eosio::contract("talk")]] message {
    uint64_t    id       = {}; // Non-0
    uint64_t    reply_to = {}; // Non-0 if this is a reply
    eosio::name user     = {};
    std::string content  = {};
    uint32_t    likes    = {};

    uint64_t primary_key() const { return id; }
    uint64_t get_reply_to() const { return reply_to; }
};

// Like table
struct [[eosio::table("like"), eosio::contract("talk")]] like {
    uint64_t    id            = {}; // Non-0
    uint64_t    post_id       = {}; // Non-0
    eosio::name user          = {};

    uint64_t primary_key()const {return id;}
    uint128_t get_user()const {return user.value;}

};

//User table
struct [[eosio::table("users"), eosio::contract("talk")]] users {
    eosio::name user       = {};
    uint32_t   num_liked   = {};
    uint32_t   num_replied = {};
    uint32_t   num_posted  = {};

    uint128_t   primary_key() const {return user.value;}
};

using message_table = eosio::multi_index<
    "message"_n, message, eosio::indexed_by<"by.reply.to"_n, eosio::const_mem_fun<message, uint64_t, &message::get_reply_to>>>;

using like_table = eosio::multi_index<
    "like"_n, like, eosio::indexed_by<"by.name"_n, eosio::const_mem_fun<like, uint128_t, &like::get_user>>>;

using users_table = eosio::multi_index<
    "users"_n, users, eosio::indexed_by<"by.user"_n, eosio::const_mem_fun<users, uint128_t, &users::primary_key>>>;



// The contract
class talk : eosio::contract {
  public:
    // Use contract's constructor
    using contract::contract;

    // Post a message
    [[eosio::action]] void post(uint64_t id, uint64_t reply_to, eosio::name user, const std::string& content) {
        message_table table{get_self(), 0};
        users_table   users{get_self(), 0};

        // Check user
        require_auth(user);

        // Check reply_to exists
        if (reply_to)
            table.get(reply_to);

        // Create an ID if user didn't specify one
        eosio::check(id < 1'000'000'000ull, "user-specified id is too big");
        if (!id)
            id = std::max(table.available_primary_key(), 1'000'000'000ull);

        // Record the message
        table.emplace(get_self(), [&](auto& message) {
            message.id       = id;
            message.reply_to = reply_to;
            message.user     = user;
            message.content  = content;
            message.likes    = 0;
        });

       // Update user table
        auto it_u = users.find(user.value);
        if (it_u == users.end()){
            //a new user
            users.emplace(get_self(), [&](auto& users){
                users.user        = user;
                users.num_liked   = 0;
                users.num_posted  = (reply_to ? 0 : 1);
                users.num_replied = (reply_to ? 1 : 0);
            });
        }else{
             users.modify(it_u, user, [&](auto&users){
                 if(reply_to)
                    users.num_replied++;
                else
                    users.num_posted++;
            });
        }
    }

    // Post a like
    [[eosio::action]] void like(uint64_t id, uint64_t post_id, eosio::name user) {

        like_table     likes{get_self(), 0};
        message_table  messages{get_self(), 0};
        users_table    users{get_self(), 0};

        // Check user
        require_auth(user);

        // Create an like ID if user didn't specify one
        eosio::check(id < 1'000'000'000ull, "user-specified id is too big");
        if (!id)
            id = std::max(likes.available_primary_key(), 1'000'000'000ull);

        //check if post exists
        auto it_m = messages.find(post_id);
        eosio::check(it_m != messages.end(), "Message not found.");

        //Check if a user tries to like their own posts.
        eosio::check(it_m->user != user, "Can't like your own posts");

        //Check if a user liked the same post before by searching like table
        bool found = false;
        auto it_l=likes.begin();
        while (it_l != likes.end())
        {
            if (it_l->post_id == post_id && it_l->user == user){
               found = true;
               break;
            }
            it_l++;
        }

        auto it_u = users.find(user.value);
        eosio::check(it_u != users.end(), "can't find the user");

        //if post was liked than unlike it
        if (found)
        {
            likes.erase(it_l);
            messages.modify(it_m, user, [&]( auto& message ) {
                message.likes = message.likes - 1;
            });
            eosio::print("post ", post_id, " was unliked by ", user);

            users.modify(it_u, user, [&](auto&users){
                users.num_liked--;
            });
        }
        else
        {
            // Record the message
            likes.emplace(get_self(), [&](auto& like) {
                like.id       = id;
                like.post_id  = post_id;
                like.user     = user;
            });
            messages.modify(it_m, user, [&]( auto& message ) {
                message.likes = message.likes + 1;
            });
            eosio::print("post ", post_id, " was liked by ", user);

            //update user table
             users.modify(it_u, user, [&](auto&users){
                users.num_liked++;
             });
        }
    }

    [[eosio::action]] void verifylikes(uint64_t id, uint32_t num) {
        message_table table{get_self(), 0};
        eosio::check(table.get(id).likes == num, "Invalid likes number");
    }

    [[eosio::action]] void numlikes(eosio::name user, uint32_t num){
        users_table users{get_self(), 0};
        eosio::check(users.get(user.value).num_liked == num, "Invalid likes number");
    }

};
