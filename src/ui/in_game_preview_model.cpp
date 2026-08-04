#include "in_game_preview_model.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace autocattery::ui {
namespace {

struct RoomCounts {
    std::size_t total{};
    std::size_t female{};
    std::size_t male{};
    std::size_t unknown{};
};

std::string RoomName(std::string_view id, bool english) {
    if (id == "Attic") return english ? "Attic" : "阁楼";
    if (id == "Floor1_Large") return english ? "1F Large" : "一楼大房";
    if (id == "Floor1_Small") return english ? "1F Small" : "一楼小房";
    if (id == "Floor2_Large") return english ? "2F Large" : "二楼大房";
    if (id == "Outside" || id.empty()) return english ? "Outside" : "房外";
    return std::string(id);
}

const char* Sex(snapshot::CatSex value, bool english) {
    if (value == snapshot::CatSex::Female) return english ? "F" : "母";
    if (value == snapshot::CatSex::Male) return english ? "M" : "公";
    return "?";
}

std::string Reason(std::string_view key, bool english) {
    if (key == "fixed-room-protection") {
        return english ? "fixed-room protection" : "遵守固定房间保护";
    }
    if (key == "recommended-breeding-pair") {
        return english ? "keep the recommended breeding pair together"
                       : "推荐繁育配对保持同房";
    }
    if (key == "sex-balance-and-potential-room") {
        return english
            ? "balance the breeding-room sexes and preserve development placement"
            : "平衡繁育房公母并保留培养位置";
    }
    if (key == "sex-balance") {
        return english ? "balance the breeding room's female/male mix"
                       : "平衡繁育房公母比例";
    }
    if (key == "high-potential-development-room") {
        return english ? "place a high-potential cat in a better room"
                       : "高潜力猫进入属性更好的房间";
    }
    if (key == "balance-room-population") {
        return english ? "balance room population" : "平衡各房人数";
    }
    return std::string(key);
}

void AddSex(RoomCounts& counts, snapshot::CatSex sex, int delta) {
    auto apply = [delta](std::size_t& value) {
        if (delta > 0) value += static_cast<std::size_t>(delta);
        else if (value > 0) --value;
    };
    apply(counts.total);
    if (sex == snapshot::CatSex::Female) apply(counts.female);
    else if (sex == snapshot::CatSex::Male) apply(counts.male);
    else apply(counts.unknown);
}

std::string CountLabel(const RoomCounts& counts, bool english) {
    std::ostringstream output;
    output << counts.total << " ("
           << (english ? "F" : "母") << counts.female << ' '
           << (english ? "M" : "公") << counts.male << " ?"
           << counts.unknown << ')';
    return output.str();
}

std::string Ratio(const RoomCounts& counts) {
    if (counts.female == 0 && counts.male == 0) return "?:?";
    return std::to_string(counts.female) + ":" +
        std::to_string(counts.male);
}

}  // namespace

DetailedPreviewModel BuildDetailedPreview(
    const workflow::PreviewBundle& bundle,
    bool english) {
    DetailedPreviewModel model;
    std::unordered_map<snapshot::CatId, const snapshot::CatSnapshot*> cats;
    std::unordered_map<
        snapshot::CatId, const classification::CatDecision*> decisions;
    std::unordered_map<std::string, RoomCounts> before;
    std::unordered_map<std::string, RoomCounts> after;
    std::vector<std::string> room_order;

    for (const auto& room : bundle.snapshot.rooms) {
        room_order.push_back(room.id);
        before.try_emplace(room.id);
    }
    for (const auto& cat : bundle.snapshot.cats) {
        cats.emplace(cat.id, &cat);
        const auto room = cat.room_id.value_or("Outside");
        if (!before.contains(room)) room_order.push_back(room);
        AddSex(before[room], cat.sex, 1);
    }
    after = before;
    for (const auto& decision : bundle.classification.decisions) {
        decisions.emplace(decision.cat_id, &decision);
    }
    for (const auto& move : bundle.room_plan.moves) {
        const auto cat = cats.find(move.cat_id);
        const auto sex = cat == cats.end()
            ? snapshot::CatSex::Unknown : cat->second->sex;
        const auto from_room = move.from_room.empty()
            ? std::string("Outside") : move.from_room;
        AddSex(after[from_room], sex, -1);
        if (!after.contains(move.to_room)) room_order.push_back(move.to_room);
        AddSex(after[move.to_room], sex, 1);
    }
    std::sort(room_order.begin(), room_order.end());
    room_order.erase(
        std::unique(room_order.begin(), room_order.end()), room_order.end());

    DetailedPreviewPage summary;
    summary.title = english ? "Full Preview · Room Summary"
                            : "完整预览 · 房间汇总";
    summary.status = english
        ? "Before and after counts show female:male ratios; no changes occur here."
        : "显示整理前后人数与母:公比例；此页面不会执行移动。";
    {
        std::ostringstream overview;
        overview << (english ? "Cats " : "猫 ") << bundle.snapshot.cats.size()
                 << " | " << (english ? "Rooms " : "房间 ")
                 << bundle.snapshot.rooms.size() << " | "
                 << (english ? "Planned moves " : "计划移动 ")
                 << bundle.room_plan.moves.size();
        summary.rows.push_back(overview.str());
    }
    for (const auto& room : room_order) {
        const auto& old_counts = before[room];
        const auto& new_counts = after[room];
        std::ostringstream row;
        row << RoomName(room, english) << " | "
            << CountLabel(old_counts, english) << " -> "
            << CountLabel(new_counts, english) << " | "
            << (english ? "ratio " : "比例 ")
            << Ratio(old_counts) << " -> " << Ratio(new_counts);
        summary.rows.push_back(row.str());
    }
    if (!bundle.room_plan.unplaced_cats.empty()) {
        summary.rows.push_back(
            (english ? "Unplaced cats: " : "未放置猫：") +
            std::to_string(bundle.room_plan.unplaced_cats.size()));
    }
    model.pages.push_back(std::move(summary));

    constexpr std::size_t kMovesPerPage = 9;
    const auto page_count = std::max<std::size_t>(
        1, (bundle.room_plan.moves.size() + kMovesPerPage - 1) /
            kMovesPerPage);
    for (std::size_t page = 0; page < page_count; ++page) {
        DetailedPreviewPage moves;
        moves.title = (english ? "Full Preview · Cat Moves "
                               : "完整预览 · 逐猫移动 ") +
            std::to_string(page + 1) + "/" + std::to_string(page_count);
        moves.status = english
            ? "Each card shows cat, sex, potential, route, and exact reason."
            : "每张卡显示猫、性别、潜力、来源、目标与移动原因。";
        const auto first = page * kMovesPerPage;
        const auto last = std::min(
            first + kMovesPerPage, bundle.room_plan.moves.size());
        for (std::size_t index = first; index < last; ++index) {
            const auto& move = bundle.room_plan.moves[index];
            const auto cat = cats.find(move.cat_id);
            const auto decision = decisions.find(move.cat_id);
            std::ostringstream row;
            row << (cat == cats.end() ? (english ? "Cat" : "猫")
                                      : cat->second->display_name)
                << " #" << move.cat_id << " | "
                << Sex(cat == cats.end() ? snapshot::CatSex::Unknown
                                         : cat->second->sex, english)
                << " | " << (english ? "potential " : "潜力 ")
                << std::fixed << std::setprecision(1)
                << (decision == decisions.end()
                        ? 0.0 : decision->second->combat_score)
                << "\n" << RoomName(move.from_room, english) << " -> "
                << RoomName(move.to_room, english) << " | "
                << Reason(move.reason, english);
            moves.rows.push_back(row.str());
        }
        if (moves.rows.empty()) {
            moves.rows.push_back(
                english ? "No cats need to move; the plan is already stable."
                        : "没有猫需要移动；当前分配已经稳定。" );
        }
        model.pages.push_back(std::move(moves));
    }
    return model;
}

}  // namespace autocattery::ui
