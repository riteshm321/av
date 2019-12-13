/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "mediametrics_tests"
#include <utils/Log.h>

#include "MediaMetricsService.h"

#include <stdio.h>

#include <gtest/gtest.h>
#include <media/MediaAnalyticsItem.h>

using namespace android;

static size_t countNewlines(const char *s) {
    size_t count = 0;
    while ((s = strchr(s, '\n')) != nullptr) {
        ++s;
        ++count;
    }
    return count;
}

TEST(mediametrics_tests, instantiate) {
  sp mediaMetrics = new MediaAnalyticsService();
  status_t status;

  // random keys ignored when empty
  std::unique_ptr<MediaAnalyticsItem> random_key(MediaAnalyticsItem::create("random_key"));
  status = mediaMetrics->submit(random_key.get());
  ASSERT_EQ(PERMISSION_DENIED, status);

  // random keys ignored with data
  random_key->setInt32("foo", 10);
  status = mediaMetrics->submit(random_key.get());
  ASSERT_EQ(PERMISSION_DENIED, status);

  // known keys ignored if empty
  std::unique_ptr<MediaAnalyticsItem> audiotrack_key(MediaAnalyticsItem::create("audiotrack"));
  status = mediaMetrics->submit(audiotrack_key.get());
  ASSERT_EQ(BAD_VALUE, status);

  // known keys not ignored if not empty
  audiotrack_key->addInt32("foo", 10);
  status = mediaMetrics->submit(audiotrack_key.get());
  ASSERT_EQ(NO_ERROR, status);


  /*
  // fluent style that goes directly to mediametrics
  ASSERT_EQ(true, MediaAnalyticsItem("audiorecord")
                     .setInt32("value", 2)
                     .addInt32("bar", 1)
                     .addInt32("value", 3)
                     .selfrecord());
  */

  mediaMetrics->dump(fileno(stdout), {} /* args */);
}

TEST(mediametrics_tests, item_manipulation) {
  MediaAnalyticsItem item("audiorecord");

  item.setInt32("value", 2).addInt32("bar", 3).addInt32("value", 4);

  int32_t i32;
  ASSERT_TRUE(item.getInt32("value", &i32));
  ASSERT_EQ(6, i32);

  ASSERT_TRUE(item.getInt32("bar", &i32));
  ASSERT_EQ(3, i32);

  item.setInt64("big", INT64_MAX).setInt64("smaller", INT64_MAX - 1).addInt64("smaller", -2);

  int64_t i64;
  ASSERT_TRUE(item.getInt64("big", &i64));
  ASSERT_EQ(INT64_MAX, i64);

  ASSERT_TRUE(item.getInt64("smaller", &i64));
  ASSERT_EQ(INT64_MAX - 3, i64);

  item.setDouble("precise", 10.5).setDouble("small", 0.125).addDouble("precise", 0.25);

  double d;
  ASSERT_TRUE(item.getDouble("precise", &d));
  ASSERT_EQ(10.75, d);

  ASSERT_TRUE(item.getDouble("small", &d));
  ASSERT_EQ(0.125, d);

  char *s;
  item.setCString("name", "Frank").setCString("mother", "June").setCString("mother", "July");
  ASSERT_TRUE(item.getCString("name", &s));
  ASSERT_EQ(0, strcmp(s, "Frank"));
  free(s);

  ASSERT_TRUE(item.getCString("mother", &s));
  ASSERT_EQ(0, strcmp(s, "July"));  // "July" overwrites "June"
  free(s);

  item.setRate("burgersPerHour", 5, 2);
  int64_t b, h;
  ASSERT_TRUE(item.getRate("burgersPerHour", &b, &h, &d));
  ASSERT_EQ(5, b);
  ASSERT_EQ(2, h);
  ASSERT_EQ(2.5, d);

  item.addRate("burgersPerHour", 4, 2);
  ASSERT_TRUE(item.getRate("burgersPerHour", &b, &h, &d));
  ASSERT_EQ(9, b);
  ASSERT_EQ(4, h);
  ASSERT_EQ(2.25, d);

  printf("item: %s\n", item.toString().c_str());
  fflush(stdout);

  sp mediaMetrics = new MediaAnalyticsService();
  status_t status = mediaMetrics->submit(&item);
  ASSERT_EQ(NO_ERROR, status);
  mediaMetrics->dump(fileno(stdout), {} /* args */);
}

TEST(mediametrics_tests, superbig_item) {
  MediaAnalyticsItem item("TheBigOne");
  constexpr size_t count = 10000;

  for (size_t i = 0; i < count; ++i) {
    item.setInt32(std::to_string(i).c_str(), i);
  }
  for (size_t i = 0; i < count; ++i) {
    int32_t i32;
    ASSERT_TRUE(item.getInt32(std::to_string(i).c_str(), &i32));
    ASSERT_EQ((int32_t)i, i32);
  }
}

TEST(mediametrics_tests, superbig_item_removal) {
  MediaAnalyticsItem item("TheOddBigOne");
  constexpr size_t count = 10000;

  for (size_t i = 0; i < count; ++i) {
    item.setInt32(std::to_string(i).c_str(), i);
  }
  for (size_t i = 0; i < count; i += 2) {
    item.filter(std::to_string(i).c_str()); // filter out all the evens.
  }
  for (size_t i = 0; i < count; ++i) {
    int32_t i32;
    if (i & 1) { // check to see that only the odds are left.
        ASSERT_TRUE(item.getInt32(std::to_string(i).c_str(), &i32));
        ASSERT_EQ((int32_t)i, i32);
    } else {
        ASSERT_FALSE(item.getInt32(std::to_string(i).c_str(), &i32));
    }
  }
}

TEST(mediametrics_tests, superbig_item_removal2) {
  MediaAnalyticsItem item("TheOne");
  constexpr size_t count = 10000;

  for (size_t i = 0; i < count; ++i) {
    item.setInt32(std::to_string(i).c_str(), i);
  }
  static const char *attrs[] = { "1", };
  item.filterNot(1, attrs);

  for (size_t i = 0; i < count; ++i) {
    int32_t i32;
    if (i == 1) { // check to see that there is only one
        ASSERT_TRUE(item.getInt32(std::to_string(i).c_str(), &i32));
        ASSERT_EQ((int32_t)i, i32);
    } else {
        ASSERT_FALSE(item.getInt32(std::to_string(i).c_str(), &i32));
    }
  }
}

TEST(mediametrics_tests, item_transmutation) {
  MediaAnalyticsItem item("Alchemist's Stone");

  item.setInt64("convert", 123);
  int64_t i64;
  ASSERT_TRUE(item.getInt64("convert", &i64));
  ASSERT_EQ(123, i64);

  item.addInt32("convert", 2);     // changes type of 'convert' from i64 to i32 (and re-init).
  ASSERT_FALSE(item.getInt64("convert", &i64));  // should be false, no value in i64.

  int32_t i32;
  ASSERT_TRUE(item.getInt32("convert", &i32));   // check it is i32 and 2 (123 is discarded).
  ASSERT_EQ(2, i32);
}

TEST(mediametrics_tests, item_binderization) {
  MediaAnalyticsItem item;
  item.setInt32("i32", 1)
      .setInt64("i64", 2)
      .setDouble("double", 3.1)
      .setCString("string", "abc")
      .setRate("rate", 11, 12);

  Parcel p;
  item.writeToParcel(&p);

  p.setDataPosition(0); // rewind for reading
  MediaAnalyticsItem item2;
  item2.readFromParcel(p);

  ASSERT_EQ(item, item2);
}

TEST(mediametrics_tests, item_byteserialization) {
  MediaAnalyticsItem item;
  item.setInt32("i32", 1)
      .setInt64("i64", 2)
      .setDouble("double", 3.1)
      .setCString("string", "abc")
      .setRate("rate", 11, 12);

  char *data;
  size_t length;
  ASSERT_EQ(0, item.writeToByteString(&data, &length));
  ASSERT_GT(length, (size_t)0);

  MediaAnalyticsItem item2;
  item2.readFromByteString(data, length);

  printf("item: %s\n", item.toString().c_str());
  printf("item2: %s\n", item2.toString().c_str());
  ASSERT_EQ(item, item2);

  free(data);
}

TEST(mediametrics_tests, item_iteration) {
  MediaAnalyticsItem item;
  item.setInt32("i32", 1)
      .setInt64("i64", 2)
      .setDouble("double", 3.125)
      .setCString("string", "abc")
      .setRate("rate", 11, 12);

  int mask = 0;
  for (auto &prop : item) {
      const char *name = prop.getName();
      if (!strcmp(name, "i32")) {
          int32_t i32;
          ASSERT_TRUE(prop.get(&i32));
          ASSERT_EQ(1, i32);
          mask |= 1;
      } else if (!strcmp(name, "i64")) {
          int64_t i64;
          ASSERT_TRUE(prop.get(&i64));
          ASSERT_EQ(2, i64);
          mask |= 2;
      } else if (!strcmp(name, "double")) {
          double d;
          ASSERT_TRUE(prop.get(&d));
          ASSERT_EQ(3.125, d);
          mask |= 4;
      } else if (!strcmp(name, "string")) {
          const char *s;
          ASSERT_TRUE(prop.get(&s));
          ASSERT_EQ(0, strcmp(s, "abc"));
          mask |= 8;
      } else if (!strcmp(name, "rate")) {
          std::pair<int64_t, int64_t> r;
          ASSERT_TRUE(prop.get(&r));
          ASSERT_EQ(11, r.first);
          ASSERT_EQ(12, r.second);
          mask |= 16;
      } else {
          FAIL();
      }
  }
  ASSERT_EQ(31, mask);
}

TEST(mediametrics_tests, item_expansion) {
  mediametrics::Item<1> item("I");
  item.set("i32", (int32_t)1)
      .set("i64", (int64_t)2)
      .set("double", (double)3.125)
      .set("string", "abcdefghijklmnopqrstuvwxyz")
      .set("rate", std::pair<int64_t, int64_t>(11, 12));
  ASSERT_TRUE(item.updateHeader());

  MediaAnalyticsItem item2;
  item2.readFromByteString(item.getBuffer(), item.getLength());
  ASSERT_EQ((pid_t)-1, item2.getPid());
  ASSERT_EQ((uid_t)-1, item2.getUid());
  int mask = 0;
  for (auto &prop : item2) {
      const char *name = prop.getName();
      if (!strcmp(name, "i32")) {
          int32_t i32;
          ASSERT_TRUE(prop.get(&i32));
          ASSERT_EQ(1, i32);
          mask |= 1;
      } else if (!strcmp(name, "i64")) {
          int64_t i64;
          ASSERT_TRUE(prop.get(&i64));
          ASSERT_EQ(2, i64);
          mask |= 2;
      } else if (!strcmp(name, "double")) {
          double d;
          ASSERT_TRUE(prop.get(&d));
          ASSERT_EQ(3.125, d);
          mask |= 4;
      } else if (!strcmp(name, "string")) {
          const char *s;
          ASSERT_TRUE(prop.get(&s));
          ASSERT_EQ(0, strcmp(s, "abcdefghijklmnopqrstuvwxyz"));
          mask |= 8;
      } else if (!strcmp(name, "rate")) {
          std::pair<int64_t, int64_t> r;
          ASSERT_TRUE(prop.get(&r));
          ASSERT_EQ(11, r.first);
          ASSERT_EQ(12, r.second);
          mask |= 16;
      } else {
          FAIL();
      }
  }
  ASSERT_EQ(31, mask);
}

TEST(mediametrics_tests, item_expansion2) {
  mediametrics::Item<1> item("Bigly");
  item.setPid(123)
      .setUid(456);
  constexpr size_t count = 10000;

  for (size_t i = 0; i < count; ++i) {
    // printf("recording %zu, %p, len:%zu of %zu  remaining:%zu \n", i, item.getBuffer(), item.getLength(), item.getCapacity(), item.getRemaining());
    item.set(std::to_string(i).c_str(), (int32_t)i);
  }
  ASSERT_TRUE(item.updateHeader());

  MediaAnalyticsItem item2;
  printf("begin buffer:%p  length:%zu\n", item.getBuffer(), item.getLength());
  fflush(stdout);
  item2.readFromByteString(item.getBuffer(), item.getLength());

  ASSERT_EQ((pid_t)123, item2.getPid());
  ASSERT_EQ((uid_t)456, item2.getUid());
  for (size_t i = 0; i < count; ++i) {
    int32_t i32;
    ASSERT_TRUE(item2.getInt32(std::to_string(i).c_str(), &i32));
    ASSERT_EQ((int32_t)i, i32);
  }
}

TEST(mediametrics_tests, time_machine_storage) {
  auto item = std::make_shared<MediaAnalyticsItem>("Key");
  (*item).set("i32", (int32_t)1)
      .set("i64", (int64_t)2)
      .set("double", (double)3.125)
      .set("string", "abcdefghijklmnopqrstuvwxyz")
      .set("rate", std::pair<int64_t, int64_t>(11, 12));

  // Let's put the item in
  android::mediametrics::TimeMachine timeMachine;
  ASSERT_EQ(NO_ERROR, timeMachine.put(item, true));

  // Can we read the values?
  int32_t i32;
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key", "i32", &i32, -1));
  ASSERT_EQ(1, i32);

  int64_t i64;
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key", "i64", &i64, -1));
  ASSERT_EQ(2, i64);

  double d;
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key", "double", &d, -1));
  ASSERT_EQ(3.125, d);

  std::string s;
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key", "string", &s, -1));
  ASSERT_EQ("abcdefghijklmnopqrstuvwxyz", s);

  // Using fully qualified name?
  i32 = 0;
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key.i32", &i32, -1));
  ASSERT_EQ(1, i32);

  i64 = 0;
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key.i64", &i64, -1));
  ASSERT_EQ(2, i64);

  d = 0.;
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key.double", &d, -1));
  ASSERT_EQ(3.125, d);

  s.clear();
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key.string", &s, -1));
  ASSERT_EQ("abcdefghijklmnopqrstuvwxyz", s);
}

TEST(mediametrics_tests, time_machine_remote_key) {
  auto item = std::make_shared<MediaAnalyticsItem>("Key1");
  (*item).set("one", (int32_t)1)
         .set("two", (int32_t)2);

  android::mediametrics::TimeMachine timeMachine;
  ASSERT_EQ(NO_ERROR, timeMachine.put(item, true));

  auto item2 = std::make_shared<MediaAnalyticsItem>("Key2");
  (*item2).set("three", (int32_t)3)
         .set("[Key1]four", (int32_t)4)   // affects Key1
         .set("[Key1]five", (int32_t)5);  // affects key1

  ASSERT_EQ(NO_ERROR, timeMachine.put(item2, true));

  auto item3 = std::make_shared<MediaAnalyticsItem>("Key2");
  (*item3).set("six", (int32_t)6)
         .set("[Key1]seven", (int32_t)7);   // affects Key1

  ASSERT_EQ(NO_ERROR, timeMachine.put(item3, false)); // remote keys not allowed.

  // Can we read the values?
  int32_t i32;
  ASSERT_EQ(NO_ERROR, timeMachine.get("Key1.one", &i32, -1));
  ASSERT_EQ(1, i32);

  ASSERT_EQ(NO_ERROR, timeMachine.get("Key1.two", &i32, -1));
  ASSERT_EQ(2, i32);

  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key1.three", &i32, -1));

  ASSERT_EQ(NO_ERROR, timeMachine.get("Key2.three", &i32, -1));
  ASSERT_EQ(3, i32);

  ASSERT_EQ(NO_ERROR, timeMachine.get("Key1.four", &i32, -1));
  ASSERT_EQ(4, i32);

  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key2.four", &i32, -1));

  ASSERT_EQ(NO_ERROR, timeMachine.get("Key1.five", &i32, -1));
  ASSERT_EQ(5, i32);

  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key2.five", &i32, -1));

  ASSERT_EQ(NO_ERROR, timeMachine.get("Key2.six", &i32, -1));
  ASSERT_EQ(6, i32);

  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key2.seven", &i32, -1));
}

TEST(mediametrics_tests, time_machine_gc) {
  auto item = std::make_shared<MediaAnalyticsItem>("Key1");
  (*item).set("one", (int32_t)1)
         .set("two", (int32_t)2)
         .setTimestamp(10);

  android::mediametrics::TimeMachine timeMachine(1, 2); // keep at most 2 keys.

  ASSERT_EQ((size_t)0, timeMachine.size());

  ASSERT_EQ(NO_ERROR, timeMachine.put(item, true));

  ASSERT_EQ((size_t)1, timeMachine.size());

  auto item2 = std::make_shared<MediaAnalyticsItem>("Key2");
  (*item2).set("three", (int32_t)3)
         .set("[Key1]three", (int32_t)3)
         .setTimestamp(11);

  ASSERT_EQ(NO_ERROR, timeMachine.put(item2, true));
  ASSERT_EQ((size_t)2, timeMachine.size());

  //printf("Before\n%s\n\n", timeMachine.dump().c_str());

  auto item3 = std::make_shared<MediaAnalyticsItem>("Key3");
  (*item3).set("six", (int32_t)6)
          .set("[Key1]four", (int32_t)4)   // affects Key1
          .set("[Key1]five", (int32_t)5)   // affects key1
          .setTimestamp(12);

  ASSERT_EQ(NO_ERROR, timeMachine.put(item3, true));

  ASSERT_EQ((size_t)2, timeMachine.size());

  // Can we read the values?
  int32_t i32;
  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key1.one", &i32, -1));
  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key1.two", &i32, -1));
  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key1.three", &i32, -1));
  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key1.four", &i32, -1));
  ASSERT_EQ(BAD_VALUE, timeMachine.get("Key1.five", &i32, -1));

  ASSERT_EQ(NO_ERROR, timeMachine.get("Key2.three", &i32, -1));
  ASSERT_EQ(3, i32);

  ASSERT_EQ(NO_ERROR, timeMachine.get("Key3.six", &i32, -1));
  ASSERT_EQ(6, i32);

  printf("After\n%s\n", timeMachine.dump().first.c_str());
}

TEST(mediametrics_tests, transaction_log_gc) {
  auto item = std::make_shared<MediaAnalyticsItem>("Key1");
  (*item).set("one", (int32_t)1)
         .set("two", (int32_t)2)
         .setTimestamp(10);

  android::mediametrics::TransactionLog transactionLog(1, 2); // keep at most 2 items
  ASSERT_EQ((size_t)0, transactionLog.size());

  ASSERT_EQ(NO_ERROR, transactionLog.put(item));
  ASSERT_EQ((size_t)1, transactionLog.size());

  auto item2 = std::make_shared<MediaAnalyticsItem>("Key2");
  (*item2).set("three", (int32_t)3)
         .set("[Key1]three", (int32_t)3)
         .setTimestamp(11);

  ASSERT_EQ(NO_ERROR, transactionLog.put(item2));
  ASSERT_EQ((size_t)2, transactionLog.size());

  auto item3 = std::make_shared<MediaAnalyticsItem>("Key3");
  (*item3).set("six", (int32_t)6)
          .set("[Key1]four", (int32_t)4)   // affects Key1
          .set("[Key1]five", (int32_t)5)   // affects key1
          .setTimestamp(12);

  ASSERT_EQ(NO_ERROR, transactionLog.put(item3));
  ASSERT_EQ((size_t)2, transactionLog.size());
}

TEST(mediametrics_tests, audio_analytics_permission) {
  auto item = std::make_shared<MediaAnalyticsItem>("audio.1");
  (*item).set("one", (int32_t)1)
         .set("two", (int32_t)2)
         .setTimestamp(10);

  auto item2 = std::make_shared<MediaAnalyticsItem>("audio.1");
  (*item2).set("three", (int32_t)3)
         .setTimestamp(11);

  auto item3 = std::make_shared<MediaAnalyticsItem>("audio.2");
  (*item3).set("four", (int32_t)4)
          .setTimestamp(12);

  android::mediametrics::AudioAnalytics audioAnalytics;

  // untrusted entities cannot create a new key.
  ASSERT_EQ(PERMISSION_DENIED, audioAnalytics.submit(item, false /* isTrusted */));
  ASSERT_EQ(PERMISSION_DENIED, audioAnalytics.submit(item2, false /* isTrusted */));

  // TODO: Verify contents of AudioAnalytics.
  // Currently there is no getter API in AudioAnalytics besides dump.
  ASSERT_EQ(4, audioAnalytics.dump(1000).second /* lines */);

  ASSERT_EQ(NO_ERROR, audioAnalytics.submit(item, true /* isTrusted */));
  // untrusted entities can add to an existing key
  ASSERT_EQ(NO_ERROR, audioAnalytics.submit(item2, false /* isTrusted */));

  // Check that we have some info in the dump.
  ASSERT_LT(4, audioAnalytics.dump(1000).second /* lines */);
}

TEST(mediametrics_tests, audio_analytics_dump) {
  auto item = std::make_shared<MediaAnalyticsItem>("audio.1");
  (*item).set("one", (int32_t)1)
         .set("two", (int32_t)2)
         .setTimestamp(10);

  auto item2 = std::make_shared<MediaAnalyticsItem>("audio.1");
  (*item2).set("three", (int32_t)3)
         .setTimestamp(11);

  auto item3 = std::make_shared<MediaAnalyticsItem>("audio.2");
  (*item3).set("four", (int32_t)4)
          .setTimestamp(12);

  android::mediametrics::AudioAnalytics audioAnalytics;

  ASSERT_EQ(NO_ERROR, audioAnalytics.submit(item, true /* isTrusted */));
  // untrusted entities can add to an existing key
  ASSERT_EQ(NO_ERROR, audioAnalytics.submit(item2, false /* isTrusted */));
  ASSERT_EQ(NO_ERROR, audioAnalytics.submit(item3, true /* isTrusted */));

  // find out how many lines we have.
  auto [string, lines] = audioAnalytics.dump(1000);
  ASSERT_EQ(lines, (int32_t) countNewlines(string.c_str()));

  printf("AudioAnalytics: %s", string.c_str());
  // ensure that dump operates over those lines.
  for (int32_t ll = 0; ll < lines; ++ll) {
      auto [s, l] = audioAnalytics.dump(ll);
      ASSERT_EQ(ll, l);
      ASSERT_EQ(ll, (int32_t) countNewlines(s.c_str()));
  }
}