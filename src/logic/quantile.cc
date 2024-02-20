#include "logic/quantile.h"
#include "tll/scheme/logic/quantile.h"

#include <algorithm>

int Quantile::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_skip = reader.getT("skip", 0u);
	_quantiles = reader.getT("quantile", std::vector<unsigned> {95});
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	std::sort(_quantiles.rbegin(), _quantiles.rend());
	for (auto q : _quantiles) {
		if (q > 100 || q == 0)
			return _log.fail(EINVAL, "Invalid quantile {}: must be in (0, 100] range", q);
	}

	if (check_channels_size<Input>(1, 1))
		return EINVAL;
	if (check_channels_size<Timer>(1, 1))
		return EINVAL;

	return Base::_init(url, master);
}

int Quantile::_open(const tll::ConstConfig &props)
{
	_data.clear();

	return Base::_open(props);
}

int Quantile::_close()
{
	for (auto & [n, b] : _data) {
		_report(n, b.local);
		_report(n, b.global, true);
	}

	return Base::_close();
}

int Quantile::callback_tag(TaggedChannel<Input> * c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	if (msg->msgid != quantile_scheme::Data::meta_id())
		return 0;

	auto data = quantile_scheme::Data::bind(*msg);
	auto key = data.get_name();
	auto value = data.get_value();

	auto it = _data.find(key);
	if (it == _data.end()) {
		_log.info("Add new bucket for '{}'", key);
		it = _data.emplace(std::string(key), _skip).first;
	}

	auto & bucket = it->second;
	unsigned idx = 1000 * log(value + 1);
	if (bucket.global.count >= 0)
		bucket.global.push(idx);
	else
		bucket.global.count++;
	bucket.local.push(idx);
	return 0;
}

int Quantile::callback_tag(TaggedChannel<Timer> *, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	for (auto & [n, b] : _data) {
		_report(n, b.local);
		_report(n, b.global, true);
	}

	return 0;
}

int Quantile::_report(std::string_view name, Quantile::Bucket &bucket, bool global)
{
	if (bucket.count <= 0)
		return 0;

	std::string_view suffix = global ? "global " : "";

	auto it = bucket.data.rbegin();
	while (*it == 0)
		it++;

	unsigned skip = 0;

	for (auto q : _quantiles) {
		unsigned qskip = bucket.count * (100 - q) / 100;
		//_log.debug("Quantile {}, skip: {}", q, qskip);
		while (qskip >= skip + *it) {
			//_log.debug("Qskip {}, skip {}, count {}", qskip, skip, *it);
			skip += *it;
			it++;
		}
		unsigned idx = bucket.data.size() - (it - bucket.data.rbegin()) - 1;
		_log.info("Quantile {}'{}' {:02}%: {}", suffix, name, q, round(exp(idx / 1000.) - 1));
	}

	if (!global)
		bucket.reset();

	return 0;
}
