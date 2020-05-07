/*
 * Semaphore (SM)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2019-2020 Udo Steinberg, BedRock Systems, Inc.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "ec.hpp"

class Sm : public Kobject, public Queue<Ec>
{
    private:
        mword counter { 0 };

        static Slab_cache cache;

        Sm (mword, unsigned);

        static inline void *operator new (size_t) noexcept
        {
            return cache.alloc();
        }

        static inline void operator delete (void *ptr)
        {
            if (EXPECT_TRUE (ptr))
                cache.free (ptr);
        }

    public:
        unsigned const id;

        NODISCARD
        static Sm *create (mword c, unsigned i = ~0U)
        {
            return new Sm (c, i);
        }

        void destroy() { delete this; }

        ALWAYS_INLINE
        inline void dn (bool zero, uint64 t)
        {
            auto ec = Ec::current;

            {   Lock_guard <Spinlock> guard (lock);

                if (counter) {
                    counter = zero ? 0 : counter - 1;
                    return;
                }

                enqueue (ec);
            }

            if (ec->block_sc()) {

                if (t)
                    ec->set_timeout (t, this);

                Sc::schedule (true);
            }
        }

        ALWAYS_INLINE
        inline bool up()
        {
            Ec *ec;

            {   Lock_guard <Spinlock> guard (lock);

                if (!dequeue (ec = head())) {

                    if (counter == ~0UL)
                        return false;

                    counter++;

                    return true;
                }
            }

            ec->release (Ec::sys_finish<Status::SUCCESS, true>);

            return true;
        }

        ALWAYS_INLINE
        inline void timeout (Ec *ec)
        {
            {   Lock_guard <Spinlock> guard (lock);

                if (!dequeue (ec))
                    return;
            }

            ec->release (Ec::sys_finish<Status::TIMEOUT>);
        }
};
