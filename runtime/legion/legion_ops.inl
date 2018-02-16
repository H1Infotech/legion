/* Copyright 2018 Stanford University, NVIDIA Corporation
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

// Included from legion_ops.h - do not include this directly

#include "legion/legion_trace.h"

namespace Legion {
  namespace Internal {

    /////////////////////////////////////////////////////////////
    // Memoizable Operation 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<typename OP>
    MemoizableOp<OP>::MemoizableOp(Runtime *rt)
      : OP(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void MemoizableOp<OP>::initialize_memoizable()
    //--------------------------------------------------------------------------
    {
      tpl = NULL;
      memo_state = NO_MEMO;
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void MemoizableOp<OP>::execute_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(memo_state == NO_MEMO || memo_state == MEMO_REQ);
#endif
      if (memo_state == MEMO_REQ)
      {
#ifdef DEBUG_LEGION
        assert(OP::trace != NULL);
        assert(OP::trace->get_physical_trace() != NULL);
#endif
        PhysicalTrace *physical_trace = OP::trace->get_physical_trace();
        tpl = physical_trace->get_current_template();
        if (tpl == NULL)
        {
          OP::trace->set_state_record();
          tpl = physical_trace->start_new_template();
        }

        if (tpl->is_replaying())
        {
#ifdef DEBUG_LEGION
          assert(OP::trace->is_replaying());
#endif
          memo_state = REPLAY;
          OP::trace->register_physical_only(this, OP::gen);
          OP::resolve_speculation();
          replay_analysis();
          return;
        }
        else
        {
#ifdef DEBUG_LEGION
          assert(OP::trace->is_recording());
#endif
          memo_state = RECORD;
        }
      }
      OP::execute_dependence_analysis();
    };

    //--------------------------------------------------------------------------
    template<typename OP>
    std::pair<unsigned,DomainPoint> MemoizableOp<OP>::get_trace_local_id() const
    //--------------------------------------------------------------------------
    {
      return std::pair<unsigned,DomainPoint>(OP::trace_local_id, DomainPoint());
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void MemoizableOp<OP>::invoke_memoize_operation(MapperID mapper_id)
    //--------------------------------------------------------------------------
    {
      Mapper::MemoizeInput  input;
      Mapper::MemoizeOutput output;
      input.traced = OP::trace != NULL;
      output.memoize = false;
      Processor mapper_proc = OP::parent_ctx->get_executing_processor();
      MapperManager *mapper = OP::runtime->find_mapper(mapper_proc, mapper_id);
      mapper->invoke_memoize_operation(&input, &output);
      if (OP::trace == NULL && output.memoize)
        REPORT_LEGION_ERROR(ERROR_INVALID_PHYSICAL_TRACING,
            "Invalid mapper output from 'memoize_operation'. Mapper requested"
            " memoization of an operation that is not being traced.");
      set_memoize(output.memoize);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void MemoizableOp<OP>::set_memoize(bool memoize)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(memo_state == NO_MEMO);
#endif
      if (memoize && !Runtime::no_tracing && !Runtime::no_physical_tracing)
        memo_state = MEMO_REQ;
    }

  }; // namespace Internal
}; // namespace Legion 