#ifndef _STRIPESTORE_HH_
#define _STRIPESTORE_HH_

#include "../inc/include.hh"
#include "../util/DistUtil.hh"
#include "Config.hh"
#include "Stripe.hh"

class StripeStore{

    private:
        // Config* _conf;
        // vector<Stripe*> _stripe_list;

    public:

        Config* _conf;
        vector<Stripe*> _stripe_list;
       
        StripeStore(Config* conf);
        ~StripeStore();
        vector<Stripe*> getStripeList();
};

#endif
