#ifndef CELL_ZONE_H
#define CELL_ZONE_H

#include "CellGroup.h"

class CellZone : public CellGroup
{
public:

    typedef std::unordered_map<Label, Ref<CellZone>> ZoneRegistry;

    CellZone(const std::string &name = "N/A",
             const std::shared_ptr<ZoneRegistry>& registry = std::make_shared<ZoneRegistry>());

    ~CellZone();

    //- Adding/removing items
    void add(const Cell &item);

    void add(const CellGroup &items);

    template <class iterator>
    void add(iterator begin, iterator end)
    {
        for(auto it = begin; it != end; ++it)
        {
            const Cell &c = *it;

            auto insert = registry_->insert(std::make_pair(c.id(), std::ref(*this)));

            if(!insert.second && this != &insert.first->second.get())
            {
                insert.first->second.get().remove(begin, end);
                registry_->insert(std::make_pair(c.id(), std::ref(*this)));
            }

            CellGroup::add(c);
        }
    }

    void remove(const Cell &item);

    void remove(const CellGroup &other);

    template <class iterator>
    void remove(iterator begin, iterator end)
    {
        for(auto it = begin; it != end; ++it)
            if(isInGroup(*it))
                registry_->erase(static_cast<const Cell&>(*it).id());

        CellGroup::remove(begin, end);
    }

    void clear();

    std::shared_ptr<ZoneRegistry> registry() const
    { return registry_; }

private:

    std::shared_ptr<ZoneRegistry> registry_;
};

#endif
