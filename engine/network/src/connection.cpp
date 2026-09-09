#include "lcu/network/connection.h"

#include "lcu/network/sequence.h"

namespace lcu::network {

Connection::Connection(f32 retransmit_interval) : retransmit_interval_(retransmit_interval) {}

u32 Connection::pending_key(Channel channel, u16 sequence) {
    return (static_cast<u32>(channel) << 16) | static_cast<u32>(sequence);
}

void Connection::send(Channel channel, std::vector<u8> payload) {
    const usize channel_index = static_cast<usize>(channel);
    const u16 seq = next_sequence_by_channel_[channel_index]++;

    PacketHeader header;
    header.type = PacketHeader::Type::Data;
    header.sequence = seq;
    header.channel = channel;

    std::vector<u8> packet = serialize_header(header);
    packet.insert(packet.end(), payload.begin(), payload.end());

    if (is_reliable(channel)) {
        pending_acks_[pending_key(channel, seq)] = PendingReliablePacket{packet, 0.0f};
    }

    outgoing_.push_back(std::move(packet));
}

void Connection::update(f32 dt) {
    for (auto& [key, pending] : pending_acks_) {
        pending.time_since_send += dt;
        if (pending.time_since_send >= retransmit_interval_) {
            pending.time_since_send = 0.0f;
            outgoing_.push_back(pending.packet);
        }
    }
}

std::vector<std::vector<u8>> Connection::take_outgoing_packets() {
    std::vector<std::vector<u8>> result = std::move(outgoing_);
    outgoing_.clear();
    return result;
}

void Connection::queue_ack(Channel channel, u16 sequence) {
    PacketHeader ack_header;
    ack_header.type = PacketHeader::Type::Ack;
    ack_header.sequence = sequence;
    ack_header.channel = channel;
    outgoing_.push_back(serialize_header(ack_header));
}

std::vector<ReceivedMessage> Connection::on_packet_received(const std::vector<u8>& raw_packet) {
    const std::optional<PacketHeader> header = deserialize_header(raw_packet.data(), raw_packet.size());
    if (!header) {
        return {};
    }

    if (header->type == PacketHeader::Type::Ack) {
        pending_acks_.erase(pending_key(header->channel, header->sequence));
        return {};
    }

    std::vector<u8> payload(raw_packet.begin() + static_cast<std::ptrdiff_t>(PacketHeader::kWireSize),
                             raw_packet.end());

    if (is_reliable(header->channel)) {
        // Always ack, even for a duplicate delivery - the sender may
        // simply not have seen our earlier ack (that's the whole reason
        // it retransmitted).
        queue_ack(header->channel, header->sequence);
    }

    std::vector<ReceivedMessage> ready;

    switch (header->channel) {
        case Channel::UnreliableUnordered: {
            ready.push_back({header->channel, std::move(payload)});
            break;
        }
        case Channel::UnreliableSequenced: {
            if (!has_seen_unreliable_sequenced_ ||
                sequence_greater_than(header->sequence, highest_unreliable_sequenced_seen_)) {
                has_seen_unreliable_sequenced_ = true;
                highest_unreliable_sequenced_seen_ = header->sequence;
                ready.push_back({header->channel, std::move(payload)});
            }
            // else: older than (or equal to) what we've already
            // delivered - stale, dropped.
            break;
        }
        case Channel::ReliableUnordered: {
            if (delivered_reliable_unordered_.insert(header->sequence).second) {
                ready.push_back({header->channel, std::move(payload)});
            }
            break;
        }
        case Channel::ReliableOrdered: {
            if (header->sequence == next_expected_ordered_) {
                ready.push_back({header->channel, std::move(payload)});
                ++next_expected_ordered_;
                // Drain any later packets that already arrived and are
                // now next in line.
                auto it = reorder_buffer_.find(next_expected_ordered_);
                while (it != reorder_buffer_.end()) {
                    ready.push_back({header->channel, std::move(it->second)});
                    reorder_buffer_.erase(it);
                    ++next_expected_ordered_;
                    it = reorder_buffer_.find(next_expected_ordered_);
                }
            } else if (sequence_greater_than(header->sequence, next_expected_ordered_)) {
                // A future packet - hold it until the gap closes.
                reorder_buffer_.emplace(header->sequence, std::move(payload));
            }
            // else: at or behind next_expected_ordered_ - already
            // delivered, a duplicate from retransmission. Dropped (the
            // ack above still goes out so the sender stops resending).
            break;
        }
    }

    return ready;
}

}  // namespace lcu::network
