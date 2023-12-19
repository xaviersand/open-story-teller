#ifndef RESOURCE_H
#define RESOURCE_H

#include <memory>
#include <string>
#include <vector>
#include <iostream>

struct Resource
{
    std::string file;
    std::string description;
    std::string format;
    std::string type;

    ~Resource() {
        std::cout << "Res deleted" << std::endl;
    }
};

// Itérateur pour parcourir les éléments filtrés
class FilterIterator {
private:
    using Iterator = std::vector<std::shared_ptr<Resource>>::const_iterator;
    Iterator current;
    Iterator end;
    std::string filterType;

public:
    FilterIterator(Iterator start, Iterator end, const std::string &type)
        : current(start), end(end), filterType(type) {
        searchNext();
    }

    // Surcharge de l'opérateur de déréférencement
    const std::shared_ptr<Resource>& operator*() const {
        return *current;
    }

    // Surcharge de l'opérateur d'incrémentation
    FilterIterator& operator++() {
        ++current;
        searchNext();
        return *this;
    }

    // Surcharge de l'opérateur d'égalité
    bool operator==(const FilterIterator& other) const {
        return current == other.current;
    }

    // Surcharge de l'opérateur de différence
    bool operator!=(const FilterIterator& other) const {
        return !(*this == other);
    }

private:
    // Fonction pour trouver le prochain élément qui correspond au filtre
    void searchNext() {

        if (filterType != "")
        {
            while (current != end && (*current)->type != filterType) {
                ++current;
            }
        }
    }
};


class IResource
{
public:
    virtual ~IResource();

};


#endif // RESOURCE_H
