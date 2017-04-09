// Copyright (c) 2014 The Cryptocoin Revival Foundation
// Copyright (c) 2015-2017 The CryptoCoderz Team / Espers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "velocity.h"
#include "bitcoinrpc.h"

/* VelocityI(int nHeight) ? i : -1
   Returns i or -1 if not found */
int VelocityI(int nHeight)
{
    int i = 0;
    i --;
    BOOST_FOREACH(int h, VELOCITY_HEIGHT)
    if( nHeight >= h )
      i++;
    return i;
}

/* Velocity(int nHeight) ? true : false
   Returns true if nHeight is higher or equal to VELOCITY_HEIGHT */
bool Velocity_check(int nHeight)
{
    printf("Checking for Velocity on block %u: ",nHeight);
    if(VelocityI(nHeight) >= 0)
    {
        printf("Velocity is currently Enabled\n");
		return true;
	}
    printf("Velocity is currently disabled\n");
	return false;
}

/* Velocity(CBlockIndex* prevBlock, CBlock* block) ? true : false
   Goes close to the top of CBlock::AcceptBlock
   Returns true if proposed Block matches constrains */
bool Velocity(CBlockIndex* prevBlock, CBlock* block)
{
    const MapPrevTx mapInputs;

    // Define values
    int64_t TXvalue = 0;
    int64_t TXinput = 0;
    int64_t TXfee = 0;
    int64_t TXcount = 0;
    int64_t TXlogic = 0;
    int64_t TXrate = 0;
    int nHeight = prevBlock->nHeight+1;
    int i = VelocityI(nHeight);
    int HaveCoins = false;
    TXrate = block->GetBlockTime() - prevBlock->GetBlockTime();
 // TEMP PATCH : FIX VELOCITY REFERENCE IMPLEMENTATION PRIORITY 1
       // Factor in TXs for Velocity constraints only if there are TXs to do so with
       if(VELOCITY_FACTOR[i] == true && TXvalue > 0)
       {
            // Set values
    BOOST_FOREACH(const CTransaction& tx, block->vtx)
    {
        TXvalue = tx.GetValueOut();
        TXinput = tx.GetValueIn(mapInputs);
        TXfee = TXinput - TXvalue;
        TXcount = block->vtx.size();
     //   TXlogic = GetPrevAccountBalance - TXinput;
        TXrate = block->GetBlockTime() - prevBlock->GetBlockTime();
    }
        // Set Velocity logic value
    if(TXlogic > 0)
    {
       HaveCoins = true;
    }
    // Check for and enforce minimum TXs per block (Minimum TXs are disabled for Espers)
    if(VELOCITY_MIN_TX[i] > 0 && TXcount < VELOCITY_MIN_TX[i])
    {
       printf("DENIED: Not enough TXs in block\n");
       return false;
    }
    // Authenticate submitted block's TXs
    if(VELOCITY_MIN_VALUE[i] > 0 || VELOCITY_MIN_FEE[i] > 0)
    {
       // Make sure we accept only blocks that sent an amount
       // NOT being more than available coins to send
       if(VELOCITY_MIN_FEE[i] > 0 && TXinput > 0)
       {
          if(HaveCoins == false)
          {
             printf("DENIED: Balance has insuficient funds for attempted TX with Velocity\n");
             return false;
          }
       }

          if(VELOCITY_MIN_VALUE[i] > 0 && TXvalue < VELOCITY_MIN_VALUE[i])
          {
             printf("DENIED: Invalid TX value found by Velocity\n");
             return false;
          }
          if(VELOCITY_MIN_FEE[i] > 0 && TXinput > 0)
          {
             if(TXfee < VELOCITY_MIN_FEE[i])
             {
                printf("DENIED: Invalid network fee found by Velocity\n");
                return false;
             }
          }
       }
    }
    // Verify minimum Velocity rate
    if( VELOCITY_RATE[i] > 0 && TXrate > VELOCITY_RATE[i] )
    {
        printf("ACCEPTED: block has met Velocity constraints\n");
    }
    // Rates that are too rapid are rejected without exception
    else if( VELOCITY_MIN_RATE[i] > 0 && TXrate < VELOCITY_MIN_RATE[i] )
    {
        printf("DENIED: Minimum block spacing not met for Velocity\n");
        return false;
    }
    // Constrain Velocity
    if(VELOCITY_EXPLICIT[i])
    {
        if(VELOCITY_MIN_TX[i] > 0)
            return false;
        if(VELOCITY_MIN_VALUE[i] > 0)
            return false;
        if(VELOCITY_MIN_FEE[i] > 0)
            return false;
    }
    // Velocity constraints met, return block acceptance
    return true;
}
