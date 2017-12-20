/* Copyright 2016 Stanford University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <cstdio>
#include <cassert>
#include <cstdlib>
#include "legion.h"
using namespace Legion;
using namespace LegionRuntime::Accessor;

/*
 * In this example we illustrate how the Legion
 * programming model supports multiple partitions
 * of the same logical region and the benefits it
 * provides by allowing multiple views onto the
 * same logical region.  We compute a simple 5-point
 * 1D stencil using the standard formula:
 * f'(x) = (-f(x+2h) + 8f(x+h) - 8f(x-h) + f(x-2h))/12h
 * For simplicity we'll assume h=1.
 */

enum TaskIDs {
  TOP_LEVEL_TASK_ID,
  INIT_FIELD_TASK_ID,
  INIT_LAUNCHER_HELPER_TASK_ID,
  COMPUTE_LAUNCHER_HELPER_TASK_ID,
  COMPUTE_TASK_ID,
  CHECK_TASK_ID,
  PAUSE_TASK_ID,
};

enum FieldIDs {
  FID_X,
  FID_Y,
  FID_VAL,
};

enum ProjIDs {
  SINGLE_PROJ = 1,
  X_PROJ_FIRST = 2,
  Y_PROJ_FIRST = 3,
  X_PROJ_SECOND = 4,
  Y_PROJ_SECOND = 5,
};

struct RectDims {
  int side_length_x;
  int side_length_y;
};

//class SingleDiffProjectionFunctor : public StructuredProjectionFunctor
class SingleDiffProjectionFunctor : public ProjectionFunctor
{
  public:
    SingleDiffProjectionFunctor(HighLevelRuntime *rt)
      : ProjectionFunctor(rt) {}

    ~SingleDiffProjectionFunctor() {}

    virtual LogicalRegion project(Context ctx, Task *task, unsigned index,
                                  LogicalRegion upper_bound,
                                  const DomainPoint &point)
    {
      assert(0);
    }

    virtual LogicalRegion project(const Mappable *mappable, unsigned index,
                                  LogicalPartition upper_bound,
                                  const DomainPoint &point)
    {
      const Point<1> one = Point<1>(1);
      const Point<1> new_point = Point<1>(point) + one;
      DomainPoint new_d_point(new_point);
      if (runtime->has_logical_subregion_by_color(upper_bound, new_d_point))
      {
        return runtime->get_logical_subregion_by_color(
            upper_bound, new_d_point);
      }
      return runtime->get_logical_subregion_by_color(upper_bound, point);
    }

    virtual unsigned get_depth() const {
      return 0;
    }
};

class XDiffProjectionFunctorFirst : public ProjectionFunctor
{
  public:
    XDiffProjectionFunctorFirst(HighLevelRuntime *rt)
      : ProjectionFunctor(rt) {}

    ~XDiffProjectionFunctorFirst() {}

    virtual LogicalRegion project(Context ctx, Task *task, unsigned index,
                                  LogicalRegion upper_bound,
                                  const DomainPoint &point)
    {
      assert(0);
    }

    virtual LogicalRegion project(const Mappable *mappable, unsigned index,
                                  LogicalPartition upper_bound,
                                  const DomainPoint &point)
    {
      return runtime->get_logical_subregion_by_color(upper_bound, point);
    }

    virtual unsigned get_depth() const {
      return 0;
    }
};

class YDiffProjectionFunctorFirst : public ProjectionFunctor
{
  public:
    YDiffProjectionFunctorFirst(HighLevelRuntime *rt)
      : ProjectionFunctor(rt) {}

    ~YDiffProjectionFunctorFirst() {}

    virtual LogicalRegion project(Context ctx, Task *task, unsigned index,
                                  LogicalRegion upper_bound,
                                  const DomainPoint &point)
    {
      assert(0);
    }

    virtual LogicalRegion project(const Mappable *mappable, unsigned index,
                                  LogicalPartition upper_bound,
                                  const DomainPoint &point)
    {
      const Point<1> one = Point<1>(1);
      const Point<1> new_point = Point<1>(point) - one;
      DomainPoint new_d_point(new_point);
      return runtime->get_logical_subregion_by_color(upper_bound, new_d_point);
    }

    virtual unsigned get_depth() const {
      return 0;
    }
};

class XDiffProjectionFunctorSecond : public ProjectionFunctor
{
  public:
    XDiffProjectionFunctorSecond(HighLevelRuntime *rt)
      : ProjectionFunctor(rt) {}

    ~XDiffProjectionFunctorSecond() {}

    virtual LogicalRegion project(Context ctx, Task *task, unsigned index,
                                  LogicalRegion upper_bound,
                                  const DomainPoint &point)
    {
      assert(0);
    }

    virtual LogicalRegion project(const Mappable *mappable, unsigned index,
                                  LogicalPartition upper_bound,
                                  const DomainPoint &point)
    {
      const Point<1> one = Point<1>(1);
      const Point<1> new_point = Point<1>(point) + one;
      DomainPoint new_d_point = DomainPoint(new_point);
      return runtime->get_logical_subregion_by_color(upper_bound, new_d_point);
    }

    virtual unsigned get_depth() const {
      return 0;
    }
};

class YDiffProjectionFunctorSecond : public ProjectionFunctor
{
  public:
    YDiffProjectionFunctorSecond(HighLevelRuntime *rt)
      : ProjectionFunctor(rt) {}

    ~YDiffProjectionFunctorSecond() {}

    virtual LogicalRegion project(Context ctx, Task *task, unsigned index,
                                  LogicalRegion upper_bound,
                                  const DomainPoint &point)
    {
      assert(0);
    }

    virtual LogicalRegion project(const Mappable *mappable, unsigned index,
                                  LogicalPartition upper_bound,
                                  const DomainPoint &point)
    {
      return runtime->get_logical_subregion_by_color(upper_bound, point);
    }

    virtual unsigned get_depth() const {
      return 0;
    }
};

void top_level_task(const Task *task,
                    const std::vector<PhysicalRegion> &regions,
                    Context ctx, Runtime *runtime)
{
  int side_length_x = 4;
  int side_length_y = 4;
  int num_iterations = 1;
  int num_subregions_x = 4; // Assumed to divide side_length_x
  int num_subregions_y = 4; // Assumed to divide side_length_y

  // say it's disjoint by default,
  // give flag for toggling to force it to compute disjointedness
  PartitionKind partition_kind = DISJOINT_KIND;

  // Check for any command line arguments
  {
      const InputArgs &command_args = Runtime::get_input_args();
    for (int i = 1; i < command_args.argc; i++)
    {
      if (!strcmp(command_args.argv[i],"-n"))
      {
        side_length_x = 1 << atoi(command_args.argv[++i]);
        side_length_y = side_length_x;
      }
      if (!strcmp(command_args.argv[i],"-nx"))
        side_length_x = 1 << atoi(command_args.argv[++i]);
      if (!strcmp(command_args.argv[i],"-ny"))
        side_length_y = 1 << atoi(command_args.argv[++i]);
      if (!strcmp(command_args.argv[i],"-b"))
      {
        num_subregions_x = 1 << atoi(command_args.argv[++i]);
        num_subregions_y = num_subregions_x;
      }
      if (!strcmp(command_args.argv[i],"-bx"))
        num_subregions_x = 1 << atoi(command_args.argv[++i]);
      if (!strcmp(command_args.argv[i],"-by"))
        num_subregions_y = 1 << atoi(command_args.argv[++i]);
      if (!strcmp(command_args.argv[i],"-i"))
        num_iterations = atoi(command_args.argv[++i]);
      if (!strcmp(command_args.argv[i],"-c"))
        partition_kind = COMPUTE_KIND;
    }
  }

  if (side_length_x % num_subregions_x != 0 ||
      side_length_y % num_subregions_y != 0)
  {
    printf("subregions per side must evenly divide side length!\n");
    assert(0);
  }

  printf("Running computation for (%d, %d) dimensions...\n",
      side_length_x, side_length_y);
  printf("Partitioning data into (%d, %d) sub-regions...\n",
      num_subregions_x, num_subregions_y);

  // For this example we'll create a single logical region with three
  // fields.  We'll initialize the field identified by 'FID_X' and 'FID_Y' with
  // our input data and then compute the value and write into 'FID_VAL'.
  Rect<2> elem_rect(Point<2>(0,0),
      Point<2>(side_length_x-1, side_length_y-1));
  IndexSpace is = runtime->create_index_space(ctx, 
                          Domain(elem_rect));
  FieldSpace fs = runtime->create_field_space(ctx);
  {
    FieldAllocator allocator = 
      runtime->create_field_allocator(ctx, fs);
    allocator.allocate_field(sizeof(int),FID_X);
    allocator.allocate_field(sizeof(int),FID_Y);
    allocator.allocate_field(sizeof(int),FID_VAL);
  }
  LogicalRegion top_lr = runtime->create_logical_region(ctx, is, fs);
  
  // Make our color_domain based on the number of subregions
  // that we want to create.  There is one less slice than the sum of the
  // sides (for double counting the corner).
  // We subtract an additional 1 when making the bounds for 0 indexing.
  int total_diag_slices = num_subregions_x + num_subregions_y - 1;
  Rect<1> color_bounds(Point<1>(0), Point<1>(total_diag_slices - 1));
  Domain color_domain = Domain(color_bounds);

  // Create two levels of partition
  // First divide the grid along the opposite diagonal
  // Then partition each of the resulting subregions to make subregions
  // of the desired size
  IndexPartition first_ip;
  LogicalPartition first_lp;
  {
    MultiDomainPointColoring d_coloring;
    const int points_per_partition_x = side_length_x/num_subregions_x;
    const int points_per_partition_y = side_length_y/num_subregions_y;
    for (int x = 0; x < num_subregions_x; x++)
    {
      for (int y = 0; y < num_subregions_y; y++)
      {
        int x_start = x * points_per_partition_x;
        int y_start = y * points_per_partition_y;
        int x_end = x_start + points_per_partition_x - 1;
        int y_end = y_start + points_per_partition_y - 1;
        Rect<2> subrect(Point<2>(x_start, y_start),Point<2>(x_end, y_end));
        d_coloring[DomainPoint(Point<1>(x + y))].insert(
          Domain(subrect));
      }
    }

    first_ip = runtime->create_index_partition(
        ctx, is, color_domain, d_coloring, partition_kind);
    first_lp = runtime->get_logical_partition(ctx, top_lr, first_ip);

    int min_subregions = (num_subregions_x < num_subregions_y ?
        num_subregions_x : num_subregions_y);

    for (Domain::DomainPointIterator itr(color_domain); itr; itr++)
    {
      int bound = itr.p[0] + 1;
      int short_offset = 0;
      int long_offset = 0;
      if (bound > total_diag_slices - min_subregions)
      {
        long_offset = bound - min_subregions;
        short_offset = bound - total_diag_slices + min_subregions - 1;
        bound = total_diag_slices - bound + 1;
      }
      else if (bound > min_subregions)
      {
        long_offset = bound - min_subregions;
        bound = min_subregions;
      }

      int x_offset, y_offset;
      if (num_subregions_x < num_subregions_y)
      {
        x_offset = short_offset;
        y_offset = long_offset;
      }
      else
      {
        x_offset = long_offset;
        y_offset = short_offset;
      }

      Rect<1> sub_color_bounds(Point<1>(-1), Point<1>(bound));
      Domain sub_color_domain = Domain(sub_color_bounds);
      DomainPointColoring sub_d_coloring;
      LogicalRegion to_partition = runtime->get_logical_subregion_by_color(ctx,
          first_lp, itr.p);
      IndexPartition sub_ip;

      for (Domain::DomainPointIterator itr2(sub_color_domain); itr2; itr2++)
      {
        if (itr2.p[0] == -1 || itr2.p[0] == bound)
        {
          // Map the first and last point to the empty domain
          Rect<2> subrect(Point<2>(0,0),Point<2>(-1,-1));
          sub_d_coloring[itr2.p] = Domain(subrect);
          continue;
        }
        int x_start = (itr2.p[0] + x_offset) * points_per_partition_x;
        int y_start =
            (bound - itr2.p[0] - 1 + y_offset) * points_per_partition_y;
        int x_end = x_start + points_per_partition_x - 1;
        int y_end = y_start + points_per_partition_y - 1;
        Rect<2> subrect(Point<2>(x_start, y_start),Point<2>(x_end, y_end));
        sub_d_coloring[itr2.p] = Domain(subrect);
      }

      sub_ip = runtime->create_index_partition(ctx,
          to_partition.get_index_space(), sub_color_domain,
          sub_d_coloring, partition_kind);
    }
  }

  for (int i = num_subregions_x + num_subregions_y - 2; i >= 0; i--)
  {
    DomainPoint init_point = DomainPoint(Point<1>(i));
    LogicalRegion init_region =
        runtime->get_logical_subregion_by_color(first_lp, init_point);

    TaskLauncher init_helper_launcher(INIT_LAUNCHER_HELPER_TASK_ID,
         TaskArgument(NULL, 0));
    init_helper_launcher.add_region_requirement(
        RegionRequirement(init_region,
                          READ_WRITE, EXCLUSIVE, top_lr));
    init_helper_launcher.add_field(0, FID_X);
    init_helper_launcher.add_field(0, FID_Y);
    init_helper_launcher.add_field(0, FID_VAL);
    runtime->execute_task(ctx, init_helper_launcher);
  }

  // We need to run a special compute task for the corner region
  LogicalRegion corner_intermediate_region =
    runtime->get_logical_subregion_by_color(first_lp,
        DomainPoint(
        Point<1>(num_subregions_x + num_subregions_y - 2)));
  LogicalPartition corner_intermediate_partition =
    runtime->get_logical_partition_by_color(corner_intermediate_region,
        DomainPoint(Point<1>(0)));
  LogicalRegion corner_region =
    runtime->get_logical_subregion_by_color(corner_intermediate_partition,
        DomainPoint(Point<1>(0)));
  LogicalRegion dummy_region_1 =
    runtime->get_logical_subregion_by_color(corner_intermediate_partition,
        DomainPoint(Point<1>(-1)));
  LogicalRegion dummy_region_2 =
    runtime->get_logical_subregion_by_color(corner_intermediate_partition,
        DomainPoint(Point<1>(1)));

  TaskLauncher compute_launcher(COMPUTE_TASK_ID,
       TaskArgument(NULL, 0));
  compute_launcher.add_region_requirement(
      RegionRequirement(dummy_region_1,
                        READ_ONLY, EXCLUSIVE, top_lr));
  compute_launcher.add_region_requirement(
      RegionRequirement(dummy_region_2,
                        READ_ONLY, EXCLUSIVE, top_lr));
  compute_launcher.add_region_requirement(
      RegionRequirement(corner_region,
                        READ_WRITE, EXCLUSIVE, top_lr));
  compute_launcher.add_field(0, FID_VAL);
  compute_launcher.add_field(1, FID_VAL);
  compute_launcher.add_field(2, FID_VAL);
  runtime->execute_task(ctx, compute_launcher);

  // Now we launch the computation to calculate the values
  for (int j = 0; j < num_iterations; j++)
  {
    for (int i = num_subregions_x + num_subregions_y - 3; i >= 0; i--)
    {
      DomainPoint compute_point = DomainPoint(Point<1>(i));
      DomainPoint data_point = DomainPoint(Point<1>(i+1));
      LogicalRegion compute_region =
          runtime->get_logical_subregion_by_color(first_lp, compute_point);
      LogicalRegion data_region =
          runtime->get_logical_subregion_by_color(first_lp, data_point);
      bool past_switch_corner = i < (num_subregions_y - 1);

      TaskLauncher helper_launcher(COMPUTE_LAUNCHER_HELPER_TASK_ID,
           TaskArgument(&past_switch_corner, sizeof(past_switch_corner)));
      helper_launcher.add_region_requirement(
          RegionRequirement(compute_region,
                            READ_WRITE, EXCLUSIVE, top_lr));
      helper_launcher.add_region_requirement(
          RegionRequirement(data_region,
                            READ_ONLY, EXCLUSIVE, top_lr));
      helper_launcher.add_field(0, FID_VAL);
      helper_launcher.add_field(1, FID_VAL);
      runtime->execute_task(ctx, helper_launcher);
    }
  }

  // Finally, we launch a single task to check the results.
  RectDims rect_dims;
  rect_dims.side_length_x = side_length_x;
  rect_dims.side_length_y = side_length_y;
  TaskLauncher check_launcher(CHECK_TASK_ID, 
      TaskArgument(&rect_dims, sizeof(RectDims)));
  check_launcher.add_region_requirement(
      RegionRequirement(top_lr, READ_ONLY, EXCLUSIVE, top_lr));
  check_launcher.add_field(0, FID_X);
  check_launcher.add_field(0, FID_Y);
  check_launcher.add_field(0, FID_VAL);
  runtime->execute_task(ctx, check_launcher);

  // Clean up our region, index space, and field space
  runtime->destroy_logical_region(ctx, top_lr);
  runtime->destroy_field_space(ctx, fs);
  runtime->destroy_index_space(ctx, is);
}

// The standard initialize field task from earlier examples
void init_field_task(const Task *task,
                     const std::vector<PhysicalRegion> &regions,
                     Context ctx, Runtime *runtime)
{
  assert(regions.size() == 1); 
  assert(task->regions.size() == 1);
  assert(task->regions[0].privilege_fields.size() == 3);

  std::set<unsigned int>::iterator fields =
      task->regions[0].privilege_fields.begin();
  FieldID fidx = *fields;
  FieldID fidy = *(++fields);
  FieldID fid_val_write = *(++fields);
  const int pointx = task->index_point.point_data[0];
  const int pointy = task->index_point.point_data[1];
  printf("Initializing fields %d and %d for block (%d, %d)...\n",
      fidx, fidy, pointx, pointy);

  RegionAccessor<AccessorType::Generic, int> accx = 
    regions[0].get_field_accessor(fidx).typeify<int>();
  RegionAccessor<AccessorType::Generic, int> accy = 
    regions[0].get_field_accessor(fidy).typeify<int>();
  RegionAccessor<AccessorType::Generic, int> acc_val_write = 
    regions[0].get_field_accessor(fid_val_write).typeify<int>();

  Rect<2> rect = runtime->get_index_space_domain(ctx, 
      task->regions[0].region.get_index_space());

  for (PointInRectIterator<2> pir(rect); pir(); pir++)
  {
    accx.write(*pir, (*pir)[0]);
    accy.write(*pir, (*pir)[1]);
    acc_val_write.write(*pir, 1);
  }
}

void init_launcher_helper_task(const Task *task,
                          const std::vector<PhysicalRegion> &regions,
                          Context ctx, Runtime *runtime)
{
  assert(regions.size() == 1);
  assert(task->regions.size() == 1);
  assert(task->regions[0].privilege_fields.size() == 3);

  LogicalRegion lr_0 = regions[0].get_logical_region();

  LogicalPartition init_partition =
      runtime->get_logical_partition_by_color(ctx, lr_0,
                  DomainPoint(Point<1>(0)));

  Rect<1> extended_rect = runtime->get_index_partition_color_space(
      init_partition.get_index_partition());
  Rect<1> rect(Point<1>(extended_rect.lo[0] + 1),
      Point<1>(extended_rect.hi[0] - 1));
  Domain init_launch_domain = Domain(rect);
  
  ArgumentMap arg_map;
  IndexLauncher init_launcher(INIT_FIELD_TASK_ID, init_launch_domain,
                              TaskArgument(NULL, 0), arg_map);
  init_launcher.add_region_requirement(
      RegionRequirement(init_partition, 0/*projection ID*/,
                        WRITE_DISCARD, EXCLUSIVE, lr_0));
  init_launcher.add_field(0, FID_X);
  init_launcher.add_field(0, FID_Y);
  init_launcher.add_field(0, FID_VAL);
  runtime->execute_index_space(ctx, init_launcher);
}

void compute_launcher_helper_task(const Task *task,
                          const std::vector<PhysicalRegion> &regions,
                          Context ctx, Runtime *runtime)
{
  assert(regions.size() == 2);
  assert(task->regions.size() == 2);
  assert(task->regions[0].privilege_fields.size() == 1);
  assert(task->regions[1].privilege_fields.size() == 1);
  assert(task->arglen == sizeof(bool));
  const bool past_switch_corner = *((const bool*)task->args);

  LogicalRegion lr_0 = regions[0].get_logical_region(); // The region to compute
  LogicalRegion lr_1 = regions[1].get_logical_region(); // The data region

  LogicalPartition compute_partition =
      runtime->get_logical_partition_by_color(ctx, lr_0,
                  DomainPoint(Point<1>(0)));
  LogicalPartition data_partition = 
      runtime->get_logical_partition_by_color(ctx, lr_1,
                  DomainPoint(Point<1>(0)));

  Rect<1> extended_rect =
      runtime->get_index_partition_color_space(
      compute_partition.get_index_partition());
  Rect<1> rect(Point<1>(extended_rect.lo[0] + 1),
      Point<1>(extended_rect.hi[0] - 1));
  Domain compute_launch_domain = Domain(rect);
  

  ProjIDs x_proj = X_PROJ_FIRST;
  ProjIDs y_proj = Y_PROJ_FIRST;
  if (past_switch_corner)
  {
    x_proj = X_PROJ_SECOND;
    y_proj = Y_PROJ_SECOND;
  }
  
  ArgumentMap arg_map;
  IndexLauncher compute_launcher(COMPUTE_TASK_ID, compute_launch_domain,
       TaskArgument(NULL, 0), arg_map);
  compute_launcher.add_region_requirement(
      RegionRequirement(data_partition, x_proj,
                        READ_ONLY, EXCLUSIVE, lr_1));
  compute_launcher.add_region_requirement(
      RegionRequirement(data_partition, y_proj,
                        READ_ONLY, EXCLUSIVE, lr_1));
  compute_launcher.add_region_requirement(
      RegionRequirement(compute_partition, 0,
                        READ_WRITE, EXCLUSIVE, lr_0));
  compute_launcher.add_field(0, FID_VAL);
  compute_launcher.add_field(1, FID_VAL);
  compute_launcher.add_field(2, FID_VAL);

  runtime->execute_index_space(ctx, compute_launcher);
}

// Compute the value triangle value for each point in the rectangle
void compute_task(const Task *task,
                  const std::vector<PhysicalRegion> &regions,
                  Context ctx, Runtime *runtime)
{
  assert(regions.size() == 3);
  assert(task->regions.size() == 3);
  assert(task->regions[0].privilege_fields.size() == 1);
  assert(task->regions[1].privilege_fields.size() == 1);
  assert(task->regions[2].privilege_fields.size() == 1);
  
  FieldID val_fid_x_diff = *(task->regions[0].privilege_fields.begin());
  FieldID val_fid_y_diff = *(task->regions[1].privilege_fields.begin());
  FieldID val_fid_curr = *(task->regions[2].privilege_fields.begin());

  RegionAccessor<AccessorType::Generic, int> x_diff_acc = 
    regions[0].get_field_accessor(val_fid_x_diff).typeify<int>();
  RegionAccessor<AccessorType::Generic, int> y_diff_acc = 
    regions[1].get_field_accessor(val_fid_y_diff).typeify<int>();
  RegionAccessor<AccessorType::Generic, int> curr_acc = 
    regions[2].get_field_accessor(val_fid_curr).typeify<int>();

  Rect<2> rect = runtime->get_index_space_domain(ctx,
      task->regions[2].region.get_index_space());

  Domain x_dom = runtime->get_index_space_domain(ctx,
      task->regions[0].region.get_index_space());
  Domain y_dom = runtime->get_index_space_domain(ctx,
      task->regions[1].region.get_index_space());
  size_t x_volume = x_dom.get_volume();
  size_t y_volume = y_dom.get_volume();

  Point<2> lo = rect.lo;
  Point<2> hi = rect.hi;
  Point<2> cur_point;
  int x_diff_val, y_diff_val;
  const Point<2> onex = Point<2>(1,0);
  const Point<2> oney = Point<2>(0,1);

  for (long int x = hi[0]; x >= lo[0]; x--)
  {
    for (long int y = hi[1]; y >= lo[1]; y--)
    {
      cur_point = Point<2>(x, y);
      const Point<2> idx_x = cur_point + onex;
      const Point<2> idx_y = cur_point + oney;
      if (x == hi[0])
      {
        if (x_volume > 0)
        {
          x_diff_val =
              x_diff_acc.read(idx_x);
        }
        else
        {
          x_diff_val = 0;
        }
      }
      else
      {
        x_diff_val =
            curr_acc.read(idx_x);
      }
      if (y == hi[1])
      {
        if (y_volume > 0)
        {
          y_diff_val =
              y_diff_acc.read(idx_y);
        }
        else
        {
          y_diff_val = 0;
        }
      }
      else
      {
        y_diff_val =
            curr_acc.read(idx_y);
      }
      int computed_val = 0;
      if (x_diff_val > y_diff_val)
      {
        computed_val = x_diff_val + 1;
      }
      else
      {
        computed_val = y_diff_val + 1;
      }
      curr_acc.write(cur_point, computed_val);
    }
  }
}

void pause_task(const Task *task,
                        const std::vector<PhysicalRegion> &regions,
                        Context ctx, Runtime *runtime)
{
  unsigned int guess = 0;
  for (int i = 0; i < 10000; i++)
  {
    guess = guess + 1;
  }
  assert(regions.size() == 2);
  assert(task->regions.size() == 2);
  assert(task->regions[0].privilege_fields.size() == 1);
  assert(task->regions[1].privilege_fields.size() == 10000/guess);
}

void check_task(const Task *task,
                const std::vector<PhysicalRegion> &regions,
                Context ctx, Runtime *runtime)
{
  assert(regions.size() == 1);
  assert(task->regions.size() == 1);
  assert(task->regions[0].privilege_fields.size() == 3);
  assert(task->arglen == sizeof(RectDims));
  RectDims rect_dims = *((RectDims *)task->args);
  const int side_length_x = rect_dims.side_length_x;
  const int side_length_y = rect_dims.side_length_y;

  std::set<unsigned int>::iterator fields =
      task->regions[0].privilege_fields.begin();
  FieldID fidx = *fields;
  FieldID fidy = *(++fields);
  FieldID fid_val = *(++fields);

  RegionAccessor<AccessorType::Generic, int> accx = 
    regions[0].get_field_accessor(fidx).typeify<int>();
  RegionAccessor<AccessorType::Generic, int> accy = 
    regions[0].get_field_accessor(fidy).typeify<int>();
  RegionAccessor<AccessorType::Generic, int> acc_val = 
    regions[0].get_field_accessor(fid_val).typeify<int>();

  Rect<2> rect = runtime->get_index_space_domain(ctx,
      task->regions[0].region.get_index_space());

  // This is the checking task so we can just do the slow path
  bool all_passed = true;
  for (PointInRectIterator<2> pir(rect); pir(); pir++)
  {
    int x = side_length_x - 1 - accx.read(*pir);
    int y = side_length_y - 1 - accy.read(*pir);
    int val = acc_val.read(*pir);
    int expected = x + y + 1;

    //printf("At point (%lld, %lld).  Checking for values %d and %d... "
    //    "expected %d, found %d\n", pir.p[0], pir.p[1], x, y, expected, val);
    
    if (expected != val)
    {
      all_passed = false;
      break;
    }
  }
  if (all_passed)
    printf("SUCCESS!\n");
  else
    printf("FAILURE!\n");
}

void registration_callback(Machine machine, HighLevelRuntime *rt,
                               const std::set<Processor> &local_procs)
{
  rt->register_projection_functor(X_PROJ_FIRST,
      new XDiffProjectionFunctorFirst(rt));
  rt->register_projection_functor(Y_PROJ_FIRST,
      new YDiffProjectionFunctorFirst(rt));
  rt->register_projection_functor(X_PROJ_SECOND,
      new XDiffProjectionFunctorSecond(rt));
  rt->register_projection_functor(Y_PROJ_SECOND,
      new YDiffProjectionFunctorSecond(rt));
  rt->register_projection_functor(SINGLE_PROJ,
      new SingleDiffProjectionFunctor(rt));
}

int main(int argc, char **argv)
{
  Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);
  Runtime::register_legion_task<top_level_task>(TOP_LEVEL_TASK_ID,
      Processor::LOC_PROC, true/*single*/, false/*index*/, AUTO_GENERATE_ID,
      TaskConfigOptions(), "top_level_task");
  Runtime::register_legion_task<init_field_task>(INIT_FIELD_TASK_ID,
      Processor::LOC_PROC, true/*single*/, true/*index*/, AUTO_GENERATE_ID,
      TaskConfigOptions(), "init_task");
  Runtime::register_legion_task<compute_launcher_helper_task>(
      COMPUTE_LAUNCHER_HELPER_TASK_ID,
      Processor::LOC_PROC, true/*single*/, false/*index*/, AUTO_GENERATE_ID,
      TaskConfigOptions(false,true,false), "compute_launcher_helper_task");
  Runtime::register_legion_task<init_launcher_helper_task>(
      INIT_LAUNCHER_HELPER_TASK_ID,
      Processor::LOC_PROC, true/*single*/, false/*index*/, AUTO_GENERATE_ID,
      TaskConfigOptions(false,true,false), "init_launcher_helper_task");
  Runtime::register_legion_task<compute_task>(COMPUTE_TASK_ID,
      Processor::LOC_PROC, true/*single*/, true/*index*/, AUTO_GENERATE_ID,
      TaskConfigOptions(true, false, false), "compute_task");
  Runtime::register_legion_task<pause_task>(PAUSE_TASK_ID,
      Processor::LOC_PROC, true/*single*/, true/*index*/, AUTO_GENERATE_ID,
      TaskConfigOptions(true, false, false), "pause_task");
  Runtime::register_legion_task<check_task>(CHECK_TASK_ID,
      Processor::LOC_PROC, true/*single*/, false/*index*/, AUTO_GENERATE_ID,
      TaskConfigOptions(), "check_task");
  HighLevelRuntime::set_registration_callback(registration_callback);

  return Runtime::start(argc, argv);
}
