#include "../ConsoleApplication.hpp"
#include "InputError.hpp"

namespace shop::ui {
void ConsoleApplication::reviewCommands(Arguments& args) {
    const auto action = args.take("действие");
    if (action == "add") {
        const auto customer = actor();
        const auto order = args.take("заказ");
        const auto product = args.take("продукт");
        const auto rating = args.number("оценка");
        const auto text = args.take("текст");
        args.finish();
        if (rating < 1 || rating > 5) {
            throw InputError("Оценка: 1..5");
        }
        output_ << "Отзыв: "
                << state_.addReview(customer, order, product, static_cast<int>(rating), text)
                << '\n';
    } else if (action == "edit") {
        const auto customer = actor();
        const auto id = args.take("отзыв");
        const auto rating = args.number("оценка");
        const auto text = args.take("текст");
        args.finish();
        if (rating < 1 || rating > 5) {
            throw InputError("Оценка: 1..5");
        }
        state_.editReview(customer, id, static_cast<int>(rating), text);
        output_ << "Отзыв отправлен на повторную модерацию\n";
    } else if (action == "vote") {
        const auto customer = actor();
        const auto id = args.take("отзыв");
        args.finish();
        state_.voteHelpful(customer, id);
        output_ << "Голос принят\n";
    } else if (action == "moderate") {
        requireOperator();
        const auto id = args.take("отзыв");
        const auto publish = args.boolean("публикация yes/no");
        const auto reason = publish ? std::string{} : args.take("причина");
        args.finish();
        state_.moderateReview(id, publish, reason);
        output_ << "Модерация выполнена\n";
    } else if (action == "rating") {
        const auto product = args.take("продукт");
        args.finish();
        const auto rating = state_.reviews().ratingSummary(product);
        output_ << "Отзывов: " << rating.count;
        if (const auto average = rating.average()) {
            output_ << " | средняя оценка " << average->first << '/' << average->second;
        }
        output_ << '\n';
    } else if (action == "list") {
        const auto product = args.take("продукт");
        const auto sort = args.empty() ? std::string("helpful") : args.take("сортировка");
        const auto offset = args.empty() ? 0 : args.number("смещение");
        const auto limit = args.empty() ? 20 : args.number("лимит");
        args.finish();
        if (offset < 0 || limit < 0 ||
            (sort != "helpful" && sort != "high" && sort != "low" && sort != "id")) {
            throw InputError("Сортировка helpful/high/low/id; пагинация неотрицательна");
        }
        const auto order = sort == "helpful" ? ReviewSort::Helpful
                           : sort == "high"  ? ReviewSort::HighestRating
                           : sort == "low"   ? ReviewSort::LowestRating
                                             : ReviewSort::Id;
        for (const auto& review :
             state_.reviews().verifiedReviewsFor(product, order, static_cast<std::size_t>(offset),
                                                 static_cast<std::size_t>(limit))) {
            output_ << review.id() << " | " << review.rating() << "/5 | полезно "
                    << review.helpfulVoters().size() << " | " << review.text() << '\n';
        }
    } else {
        throw InputError("Неизвестная команда отзывов");
    }
}
} // namespace shop::ui
