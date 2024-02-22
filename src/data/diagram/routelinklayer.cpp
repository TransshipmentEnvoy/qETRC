#include "routelinklayer.h"
#include <QDebug>

typename RouteLinkLayer::item_set_t::const_iterator 
	RouteLinkLayer::occupationInRange(int start_x, int end_x) const
{
	auto itr = _items.lower_bound(start_x);
	if (itr == _items.end()) return itr;
	qDebug() << "Found occupation: " << itr->start_x << ", " << itr->end_x;
	// has intersection, return the iterator
	if (std::max(itr->start_x, start_x) < std::min(itr->end_x, end_x))
		return itr;

	// it seems that only the first iterator could cause conflict, if the layer contains no
	// conflict before this test.
	// The only possible case for the first itr that not conflict with the input range
	// is that the range specified by itr is completely after the given range.
	else return _items.end();

}

typename RouteLinkLayer::item_set_t::const_iterator 
	RouteLinkLayer::occupationInRangePBC(int start_x, int end_x, const int width) const
{
	if (start_x > end_x) {
		auto itr = occupationInRange(start_x, width);
		return itr == _items.end() ? occupationInRange(0, end_x) : itr;
	}
	else {
		return occupationInRange(start_x, end_x);
	}
}

bool RouteLinkLayer::isOccupied(int start_x, int end_x) const
{
	return occupationInRange(start_x, end_x) != _items.end();
}

bool RouteLinkLayer::isOccupiedPBC(int start_x, int end_x, const int width) const
{
	return occupationInRangePBC(start_x, end_x, width) != _items.end();
}

void RouteLinkLayer::addOccupation(const RouteLinkOccupy& occ, const int width)
{
	if (occ.start_x > occ.end_x) {
		_items.emplace(occ.train, occ.start_x, width);
		_items.emplace(occ.train, 0, occ.end_x);
	}
	else {
		_items.emplace(occ);
	}
}

int RouteLinkLayerManager::addOccupation(const RouteLinkOccupy& occ, const int width)
{
	for (int i = 0; i < (int)_layers.size(); i++) {
		auto& lay = _layers.at(i);
		if (!lay.isOccupiedPBC(occ.start_x, occ.end_x, width)) {
			lay.addOccupation(occ, width);
			return i;
		}
	}
	// Now, add to newly created layer
	RouteLinkLayer lay;
	lay.addOccupation(occ, width);
	_layers.emplace_back(std::move(lay));
	return _layers.size() - 1;
}
