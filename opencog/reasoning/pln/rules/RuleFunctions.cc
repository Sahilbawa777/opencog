#include <platform.h>
#include "../PLN.h"

#include "Rule.h"
#include "Rules.h"
#include "../AtomTableWrapper.h"
#include "../PLNatom.h"
#include "../BackInferenceTreeNode.h"

namespace haxx
{
    /// Must invert the formula I*(I+1)/2 for 2-level incl. excl. in OR rule

    map<int, int> total2level1;
    int contractInclusionExclusionFactorial(int total)
    {
        map<int, int>::iterator T2L = total2level1.find(total);

        if(T2L != total2level1.end())
            return T2L->second;

        int val=0;
        int i=0;
        
        for (i = total2level1.size(); val != total; i++)
            val = total2level1[i] = i*(i+1)/2;

        return i-1;
    }
}

namespace reasoning
{

#ifndef WIN32
  float max(float a, float b) { return ((a>b)?a:b); }
#endif

extern int varcount;

Vertex CreateVar(iAtomTableWrapper* atw, std::string varname)
{
    Handle ret = atw->addNode(FW_VARIABLE_NODE,varname,
        TruthValue::TRIVIAL_TV(),false,false);
    
cprintf(4, "CreateVar Added node as NEW: %s / [%lu]\n", varname.c_str(), (ulong) ret);

    varcount++;

    return Vertex(ret);
}

Vertex CreateVar(iAtomTableWrapper* atw)
{
    return CreateVar(atw, "$"+GetRandomString(10));
}

Rule::setOfMPs makeSingletonSet(Rule::MPs& mp)
{
    Rule::setOfMPs ret;
    ret.insert(mp);
    return ret;
}

// Redundant, hopefully:
BBvtree atomWithNewType(Handle h, Type T)
{
    assert(GET_ATW->inheritsType(T, LINK));
    AtomSpace *nm = CogServer::getAtomSpace();
    vector<Handle> children = nm->getOutgoing(h);
    BBvtree ret_m(new BoundVTree(mva((Handle)T)));
    foreach(Handle c, children)
    {
        ret_m->append_child(ret_m->begin(), mva(c).begin());
    }
    
    return ret_m;
}
    
BBvtree atomWithNewType(const tree<Vertex>& v, Type T)
{
    Handle *ph = v2h(&*v.begin());
// TODO just call the overloaded Vertex version below
    AtomSpace *nm = CogServer::getAtomSpace();
    if (!ph || !nm->isReal(*ph)) //Virtual: just replace the root node
    {
    
        BBvtree ret_m(new BoundVTree(v));
        *ret_m->begin() = Vertex((Handle)T);
        return ret_m;
    }
    else //Real: construct a new tree from T root and v's outgoing set.
    {
        assert(GET_ATW->inheritsType(T, LINK));

        vector<Handle> children = nm->getOutgoing(*ph);
        BBvtree ret_m(new BoundVTree(mva((Handle)T)));
        foreach(Handle c, children)
        {
            ret_m->append_child(ret_m->begin(), mva(c).begin());
        }
        
        return ret_m;
    }   
}

BBvtree atomWithNewType(const Vertex& v, Type T)
{
    Handle *ph = (Handle*) v2h(&v);
    AtomSpace *nm = CogServer::getAtomSpace();
    if (!ph || !nm->isReal(*ph)) //Virtual: just replace the root node
    {
    
        BBvtree ret_m(new BoundVTree(v));
        *ret_m->begin() = Vertex((Handle)T);
        return ret_m;
    }
    else //Real: construct a new tree from T root and v's outgoing set.
    {
        assert(GET_ATW->inheritsType(T, LINK));

        vector<Handle> children = nm->getOutgoing(*ph);
        BBvtree ret_m(new BoundVTree(mva((Handle)T)));
        foreach(Handle c, children)
        {
            ret_m->append_child(ret_m->begin(), mva(c).begin());
        }
        
        return ret_m;
    }   
}

/// haxx:: \todo VariableNodes not memory-managed.
bool UnprovableType(Type T)
{
    return  //inheritsType(T, OR_LINK) ||
            GET_ATW->inheritsType(T, CONCEPT_NODE) ||
            GET_ATW->inheritsType(T, FORALL_LINK);
}

template<Type T>
Handle Join(Handle* h, int N, AtomTableWrapper& atw)
{
    vector<Handle> hs;
    for (int i = 0; i < N; i++)
        hs.push_back(h[i]);

    return atw.addLink(T, hs, TruthValue::TRUE_TV(), false);
}

template<Type T, typename ATW>
Handle Join(Handle h1, Handle h2, ATW& atw)
{
    Handle h[] = { h1, h2 };
    return Join<T>(h, 2, atw);
}

void insertAllANDCombinations(set<atom, lessatom_ignoreVarNameDifferences> head, vector<atom> tail, set<atom, lessatom_ignoreVarNameDifferences>& AND_combinations)
{
    int totalSize = tail.size() + head.size();

    if (totalSize < 2)
        return;

    if (tail.empty())
    {
        atom neBoundVertex(AND_LINK, 0);
        neBoundVertex.hs = vector<Btr<atom> >(head.size());

        if (!head.empty())
            foreach(const atom &a, head)
                neBoundVertex.hs.push_back(Btr<atom>(new atom(a)));
//          copy(head.begin(), head.end(), neBoundVertex.hs.begin());

        AND_combinations.insert(neBoundVertex);
    }
    else
    {
        atom natom = tail.back();
        tail.pop_back();
        
        /// Combinations without natom

        if (totalSize >= 3) //If ANDLink producible even if 1 element was removed.
            insertAllANDCombinations(head, tail, AND_combinations);
        
        head.insert(natom);

        /// Combinations with natom

        insertAllANDCombinations(head, tail, AND_combinations);
    }
}

template<typename C>
void createPermutation(vector<C> seed, set< vector<C> >& result, int index)
{
    if (index >= seed.size()-1)
        result.insert(seed);
    else
    {
        for (int i = index+1; i < seed.size(); i++)
        {
            createPermutation<C>(seed, result, index+1);

            //C temp = seed[i];
            swap<C>(seed[index], seed[i]);

            createPermutation<C>(seed, result, index+1);

            swap<C>(seed[index], seed[i]); //Cancel
        }
    }
}

template<typename C >
set<vector<C> >* newCreatePermutations(vector<C> seed)
{
    set<vector<C> >* ret = new set<vector<C> >;
    createPermutation<C>(seed, *ret, 0);

    return ret;
}

Handle UnorderedCcompute(iAtomTableWrapper *destTable, Type linkT, const ArityFreeFormula<TruthValue,
             TruthValue*>& fN, Handle* premiseArray, const int n, Handle CX)
{
        TruthValue** tvs = new TruthValue*[n];
        for (int i = 0; i < n; i++)
            tvs[i] = (TruthValue*) &(GET_ATW->getTV(premiseArray[i]));
            //tvs[i] = (TruthValue*) destTable->getTruthValue(premiseArray[i]);
//puts("Computing formula");
        TruthValue* retTV = fN.compute(tvs, n);
//puts("Creating outset");
        HandleSeq outgoing;
        for (int j = 0; j < n; j++)
            outgoing.push_back(premiseArray[j]);
//puts("Adding link");
        Handle ret = destTable->addLink(linkT, outgoing,
            *retTV,
            RuleResultFreshness);   

        /// Release
        
        delete[] tvs;
        delete retTV;

        return ret;
}
    
Rule::setOfMPs PartitionRule_o2iMetaExtra(meta outh, bool& overrideInputFilter, Type OutLinkType)
{
        const int N = outh->begin().number_of_children();
        
        if (!GET_ATW->inheritsType(GET_ATW->getType(v2h(*outh->begin())), OutLinkType) ||
            N <= MAX_ARITY_FOR_PERMUTATION)
            return Rule::setOfMPs();

        Rule::MPs ret;

        int parts = N / MAX_ARITY_FOR_PERMUTATION;
        int remainder = N % MAX_ARITY_FOR_PERMUTATION;

        vector<atom> hs;
        tree<Vertex>::sibling_iterator ptr = outh->begin(outh->begin());
        BBvtree root(new BoundVTree);
        root->set_head((Handle)OutLinkType);

        for (int i = 0; i <= parts; i++)
        {
            int elems = ( (i<parts) ? MAX_ARITY_FOR_PERMUTATION : remainder);
            
            BoundVTree elem(mva((Handle)AND_LINK));
            
            if (elems>1)
            {
                for (int j = 0; j < elems; j++)
                    elem.append_child(elem.begin(), *ptr++);
                root->append_child(root->begin(), elem.begin());
            }
            else if (elems>0)
                root->append_child(root->begin(), *ptr++);
        }

        assert(ptr == outh->end(outh->begin()));

        ret.push_back(root);

        overrideInputFilter = true;
        
        return makeSingletonSet(ret);
}

/* TODO: Update to BoundVTree
boost::shared_ptr<set<BoundVertex > > attemptDirectANDProduction(iAtomTableWrapper *destTable,
                                        const meta& outh,
                                        ArityFreeANDRule* rule)
    {
        if (!inheritsType(nm->getType(v2h(*outh->begin())), AND_LINK))
            return Rule::setOfMPs();

        tree<Vertex>::iterator top = outh->begin();
        const int N = outh->begin(top);

        set<atom, lessatom_ignoreVarNameDifferences> qnodes, AND_combinations;
        vector<atom> units;

        /// Add Individual nodes
    
        for (tree<Vertex>::sibling_iterator i = outh->begin(top);
                                            i!= outh->end  (top);
                                            i++)
            qnodes.insert(v2h(*i));

        AND_combinations = qnodes;

        /// Add ANDLinks

        insertAllANDCombinations(set<atom, lessatom_ignoreVarNameDifferences>(), outh.hs, AND_combinations);

        /// Create ring MP

        meta gatherMP(new BoundVTree((Handle)OR_LINK));

        if (!AND_combinations.empty())
        {
            //gatherMP.hs = vector<atom>(AND_combinations.size());
            for (set<atom, lessatom_ignoreVarNameDifferences>::iterator a =
                AND_combinations.begin(); a != AND_combinations.end(); a++)
            {
                gatherMP.append_child(  gatherMP.begin(),
                                        a->maketree().begin());
            }
            //copy(AND_combinations.begin(), AND_combinations.end(), gatherMP.hs.begin());
        }

        /// Gather

        Handle premises[50];
        
        TableGather nodeInstances(gatherMP, destTable, -1);
                                                
        for (int ni=0;ni < nodeInstances.size(); ni++)
        {
            premises[ni] = nodeInstances[ni];

            atom a(nodeInstances[ni]);

            if (a.T == AND_LINK)
                for (vector<atom>::iterator i = a.hs.begin(); i!=a.hs.end();i++)
                    qnodes.erase(*i);
            else
                qnodes.erase(a);
        }

        if (qnodes.empty()) //All found
            return rule->compute(premises, nodeInstances.size());
        else
            return Vertex((Handle)NULL);

}*/

/*
  [IntInh A B].tv.s = [ExtInh Ass(B) Ass(A)].tv.s
  Ass(A) = { c: [p(C|A) - P(C|-A)] > 0 }
  Wrong ExtInh direction?
  */

/** TODO: Update to tree<Vertex> && new rule storage (not ATW)
Handle Ass(iAtomTableWrapper *destTable, Handle h, vector<Handle>& ret)
{
    TableGather g(mva((Handle)INHERITANCE_LINK,
        mva(CreateVar(destTable)),
        mva(h)));
    TableGather reverseg(mva((Handle)INHERITANCE_LINK,
        mva(h),
        mva(CreateVar(destTable))));

    for(vector<Handle>::iterator i = reverseg.begin(); i != reverseg.end(); i++)
    {
        Handle hs[] = { *i };
        *i = AtomTableWrapper::rule[Inversion_Inheritance]->compute(hs,1);
    }
    
    ret = reverseg;
    for (vector<Handle>::iterator j = g.begin(); j != g.end();j++)
        ret.push_back(*j);

    string ass_name = nm->getName(h);

    LOG(0, "Ass of " + ass_name + ":");
    for_each<vector<Handle>::iterator, handle_print<0> >(ret.begin(),
        ret.end(), handle_print<0>());

    atom ass(CONCEPT_NODE, "ASS(" + (ass_name.empty()?GetRandomString(10):ass_name) + ")");

    /// What's the correct TV of the ASS conceptnode?

    for (vector<Handle>::iterator k = ret.begin(); k != ret.end();k++)
    {
        TruthValue* tv = getTruthValue(*k);

        printTree(
            destTable->addAtom(
                atom(MEMBER_LINK, 2,
                    new atom(nm->getOutgoing(*k)[0]),
                    new atom(ass)
                ),
                tv->clone(),
                                true);  
//                              false);
            )
            ,0,0
        );
    }

    return ass.attach(destTable);
}*/

void pr(pair<Handle, Handle> i);
void pr2(pair<Handle, vtree> i);


template <class InputIterator,
                        class OutputIterator,
                        class UnaryOperation,
                        class UnaryPredicate>
OutputIterator transform_if(InputIterator first,
                                        InputIterator last,
                                        OutputIterator result,
                                        UnaryOperation op,
                                        UnaryPredicate test)
{
        while (first != last) {
                if (test(*first))
                        *result++ = op(*first);
                     else
                                *result++ = *first;
                ++first;
        }
        return result;
}

template<typename T>
struct returnor  : public unary_function<T,T> 
{
    const T& ret;
    returnor(const T& _ret) : ret(_ret) {}
    T operator()(const T& _arg) const { return ret; }
};

template<typename T>
struct converter  : public binary_function<T,T,void> 
{
    T src, dest;
    converter(const T& _src, const T& _dest) : src(_src), dest(_dest) {}
    void operator()(T& _arg) {
        if (_arg == src)
            _arg = dest; }
};

Btr<vtree> convert_all_var2fwvar(vtree vt_const, iAtomTableWrapper* table)
{
    AtomSpace *nm = CogServer::getAtomSpace();

    Btr<vtree> ret(new vtree(vt_const));
/// USE THIS IF YOU WISH TO CONVERT VARIABLE_NODEs to FW_VARIABLE_NODEs!
    vtree::iterator vit = ret->begin();

    while(  (vit =  find_if(ret->begin(), ret->end(),
                                bind(equal_to<Type>(),
                                    bind(getTypeVFun, _1),
                                    (Type)(int)VARIABLE_NODE
                                )
                            )
                ) != ret->end())
    {
        Vertex new_var = CreateVar(table);
        for_each(ret->begin(), ret->end(), converter<Vertex>(*vit, new_var));
#if 0
        printTree(v2h(*vit),0,4);
        rawPrint(*ret, ret->begin(), 4);
#else 
        NMPrinter printer(NMP_HANDLE|NMP_TYPE_NAME);
        printer.print(v2h(*vit), 4);
        printer.print(ret->begin(), 4);
#endif
    }

    return ret;
}

//Btr<ModifiedVTree> convertToModifiedVTree(Btr<vtree> vt)
Btr<ModifiedVTree> convertToModifiedVTree(Handle h, Btr<vtree> vt)
{
    return Btr<ModifiedVTree>(new ModifiedVTree(*vt, h));
}
/*Btr<ModifiedVTree> convertToModifiedVTree(vtree& vt)
{
    return Btr<ModifiedVTree>(new ModifiedVTree(vt));
}*/

/*template<typename T2, typename T3>
struct mapper
{
    const map<T2, T3>& m;

    mapper(const map<T2, T3> _m) : m(_m) {}
    T3 operator()(const T2& key)
    {
        map<T2, T3>::const_iterator i = m.find(key);
        if (i == m.end())
            return key;
        else
            return i->second;
    }
};*/

template<typename T, typename T2>
bool stlhasi(const T& c, const T2& k)
{
    return c.find(k) != c.end();
}

/**
    After a StrictCrispU Node has been created here, it contains some newly converted FW_VARs.
    We have to convert them to FW_VARs to avoid complexities of having to deal with 2 types of
    vars everywhere.
    But we must be able to bind those vars later, by spawning a new copy of this Inference Node.
    But if we do that, then we must be able to connect the FW_VARs with this node.
    But we cannot simply fix the conversion mapping from VARs to FW_VARs for each ForAllLink,
    because we want to be able to use the same ForAllLink several times during the same inference
    chain!
    
*/

template<typename T1, typename bindContainerIterT, typename TM>
bool consistent_bindingsVTreeT(TM& b1, bindContainerIterT b2start, bindContainerIterT b2end)
{
    for (bindContainerIterT b = b2start;
        b!= b2end;
        b++)
    {
        bindContainerIterT bit;

        if ((bit = b1.find(b->first)) != b1.end() &&
            !(bit->second == b->second))
        {
            return false;

/*          ///The same var bound different way. First virtualize them:

            vtree binder1(make_vtree(v2h(b->second)));
            vtree binder2(make_vtree(v2h(bit->second)));

            /// Then apply all bindings on both sides to both, to "normalize away" dependencies

            Btr<vtree> binder1A(tree_transform(binder1,   mapper<T1, bindContainerIterT>(b1, b1start, b1end)));
            Btr<vtree> binder1B(tree_transform(*binder1A, mapper<T1, bindContainerIterT>(b2, b2start, b2end)));
            Btr<vtree> binder2A(tree_transform(binder2,   mapper<T1, bindContainerIterT>(b1, b1start, b1end)));
            Btr<vtree> binder2B(tree_transform(*binder2A, mapper<T1, bindContainerIterT>(b2, b2start, b2end)));

            return *binder2B == *binder1B; //Check if it's still inconsistent.*/
        }
    }

    return true;
}

void insert_with_consistency_check_bindingsVTreeT(map<Handle, vtree>& m, map<Handle, vtree>::iterator rstart, map<Handle, vtree>::iterator rend)
{
    if (consistent_bindingsVTreeT<Vertex>(m, rstart, rend))
        m.insert(rstart, rend);
    else
        throw PLNexception("InconsistentBindingException");
}

Btr<set<Handle> > ForAll_handles;

Btr<ModifiedBoundVTree> FindMatchingUniversal(meta target, Handle ForAllLink, iAtomTableWrapper* table)
{
	cprintf(4,"FindMatchingUniversal...");
	
    Btr<ModifiedVTree> candidate = 
        convertToModifiedVTree(
                            ForAllLink,
                            convert_all_var2fwvar(
                                make_vtree(getOutgoingFun()(ForAllLink, 1)),
                                table));

    Btr<bindingsVTreeT> bindsInUniversal    (new bindingsVTreeT),
                                bindsInTarget       (new bindingsVTreeT);

    NMPrinter printer(NMP_HANDLE|NMP_TYPE_NAME);
    printer.print(candidate->begin(), 4);
    printer.print(target->begin(), 4);

    if (!unifiesTo(*candidate, *target, *bindsInUniversal, *bindsInTarget, true)) //, VARIABLE_NODE))
        return Btr<ModifiedBoundVTree>();

    printer.print(candidate->begin(), 4);
    printer.print(candidate->original_handle, 4);

	cprintf(4,"universal binds:");
    for_each(bindsInUniversal->begin(), bindsInUniversal->end(), pr2);
	cprintf(4,"target binds:");
    for_each(bindsInTarget->begin(), bindsInTarget->end(), pr2);

    Btr<ModifiedBoundVTree> BoundUniversal(new ModifiedBoundVTree(*candidate));
    /// bindsInTarget will later become constraints/pre-bindings of the new BIN.
    /// UPDATE: THIS FEATURE WAS CANCELCED:
    /// BoundUniversal->bindings = bindsInTarget;
    BoundUniversal->bindings.reset(new bindingsVTreeT);

    Btr<bindingsVTreeT> bindsCombined(new bindingsVTreeT(*bindsInUniversal));
    insert_with_consistency_check_bindingsVTreeT(*bindsCombined, bindsInTarget->begin(), bindsInTarget->end());

    ///Remove FW_VAR => FW_VAR mappings. WARNING! Potentially goes to infinite loop.
    removeRecursionFromMap<bindingsVTreeT::iterator, vtree::iterator>(bindsCombined->begin(), bindsCombined->end());

	cprintf(4,"\ncombined binds:");
    for_each(bindsCombined->begin(), bindsCombined->end(), pr2);

    /**     /// Previously:
    /// Combined bindings are applied to the candidate
    for(vtree::iterator v = BoundUniversal->begin();
    v!= BoundUniversal->end();)
    {
    bindingsVTreeT::iterator it = bindsCombined->find(v2h(*v));
    if (it != bindsCombined->end())
    {
    BoundUniversal->replace(v, it->second.begin());
    v = BoundUniversal->begin();
    }
    else
    ++v;
    }*/

    printer.print(BoundUniversal->begin(), 4);

    /// Previously:
    /// All recursion was removed from the combined binds, but only the binds pertaining
    /// to the variables that occurred in target are relevant and will hence be included.

    /*          for (bindingsVTreeT::iterator it = bindsCombined->begin();
    it!= bindsCombined->end();
    it++)
    if (STLhas(*bindsInTarget, it->first))
    (*BoundUniversal->bindings)[it->first] = it->second;*/

    /// Now:
    /// I changed the unifiesTo function so that lhs/rhs binding distinction was blurred.
    /// TODO: Verify that this is correct: now including bindings on both sides!

    BoundUniversal->bindings = bindsCombined;

	cprintf(4,"\nresult binds:");
    for_each(BoundUniversal->bindings->begin(), BoundUniversal->bindings->end(), pr2);

    return BoundUniversal;
}

Btr< set<Btr<ModifiedBoundVTree> > > FindMatchingUniversals(meta target, iAtomTableWrapper* table)
{
    DeclareBtr(set<Btr<ModifiedBoundVTree> >, ret);

    if (!ForAll_handles)
    {
            ForAll_handles = table->getHandleSet(FORALL_LINK, "");
        puts("recreated ForAll_handles");
        getc(stdin);
    }

    foreach(Handle h, *ForAll_handles)
    {
        Btr<ModifiedBoundVTree> BoundUniversal = FindMatchingUniversal(target, h, table);
        if (BoundUniversal)
            ret->insert(BoundUniversal);
    }

    return ret;
}

#if 0
Btr< set<Btr<ModifiedBoundVTree> > > FindMatchingUniversals(meta target, iAtomTableWrapper* table)
{
    DeclareBtr(set<Btr<ModifiedBoundVTree> >, ret);
    Btr< set<Btr<ModifiedVTree> > > ForAllLinkRepo;

    if (!ForAllLinkRepo)
    {
        if (!ForAll_handles)
            ForAll_handles = table->getHandleSet(FORALL_LINK, "");

        ForAllLinkRepo.reset(new set<Btr<ModifiedVTree> >);

        /// Handle => vtree and then convert VariableNodes to FW_VariableNodes

        transform(  ForAll_handles->begin(),
                        ForAll_handles->end(),
                        inserter(*ForAllLinkRepo, ForAllLinkRepo->begin()),
                        bind(&convertToModifiedVTree,
                            _1,
                            bind(&convert_all_var2fwvar,
                                bind(&make_vtree,
                                    bind(getOutgoingFun(), _1, 1)),
                                table))); /// Ignore arg #0 ListLink)

#if 0
        foreach(const Btr<ModifiedVTree>& mvt, *ForAllLinkRepo)
            rawPrint(*mvt, mvt->begin(),3);
#else 
        NMPrinter printer(NMP_HANDLE|NMP_TYPE_NAME);
        foreach(const Btr<ModifiedVTree>& mvt, *ForAllLinkRepo)
            printer.print(mvt->begin(),3);
#endif
        /*foreach(Handle h, *ForAll_handles)
            printTree(h, 0,0);*/
    }

    foreach(const Btr<ModifiedVTree>& candidate, *ForAllLinkRepo)
    {
        Btr<bindingsVTreeT> bindsInUniversal    (new bindingsVTreeT),
                            bindsInTarget       (new bindingsVTreeT);

#if 0
        rawPrint(*candidate, candidate->begin(), 4);
        rawPrint(*target, target->begin(), 4);
#else 
        printer.print(candidate->begin(), 4);
        printer.print(target->begin(), 4);
#endif

        if (unifiesTo(*candidate, *target, *bindsInUniversal, *bindsInTarget, true)) //, VARIABLE_NODE))
        {
#if 0
            rawPrint(*candidate, candidate->begin(), 4);
            printTree(candidate->original_handle,0,4);
#else 
            printer.print(candidate->begin(), 4);
            printer.print(candidate->original_handle, 4);
#endif

            cprintf(4,"universal binds:");
            for_each(bindsInUniversal->begin(), bindsInUniversal->end(), pr2);
            cprintf(4,"target binds:");
            for_each(bindsInTarget->begin(), bindsInTarget->end(), pr2);

            Btr<ModifiedBoundVTree> BoundUniversal(new ModifiedBoundVTree(*candidate));
            /// bindsInTarget will later become constraints/pre-bindings of the new BIN.
            /// UPDATE: THIS FEATURE WAS CANCELCED:
            /// BoundUniversal->bindings = bindsInTarget;
            BoundUniversal->bindings.reset(new bindingsVTreeT);

            Btr<bindingsVTreeT> bindsCombined(new bindingsVTreeT(*bindsInUniversal));
            insert_with_consistency_check_bindingsVTreeT(*bindsCombined, bindsInTarget->begin(), bindsInTarget->end());

            ///Remove FW_VAR => FW_VAR mappings. WARNING! Potentially goes to infinite loop.
            removeRecursionFromMap<bindingsVTreeT::iterator, vtree::iterator>(bindsCombined->begin(), bindsCombined->end());

            cprintf(4,"\ncombined binds:");
            for_each(bindsCombined->begin(), bindsCombined->end(), pr2);

/**     /// Previously:
            /// Combined bindings are applied to the candidate
            for(vtree::iterator v = BoundUniversal->begin();
                                        v!= BoundUniversal->end();)
            {
                bindingsVTreeT::iterator it = bindsCombined->find(v2h(*v));
                if (it != bindsCombined->end())
                {
                    BoundUniversal->replace(v, it->second.begin());
                    v = BoundUniversal->begin();
                }
                else
                    ++v;
            }*/

#if 0
            rawPrint(*BoundUniversal, BoundUniversal->begin(), 4);
#else 
            printer.print(BoundUniversal->begin(), 4);
#endif

            /// Previously:
            /// All recursion was removed from the combined binds, but only the binds pertaining
            /// to the variables that occurred in target are relevant and will hence be included.

/*          for (bindingsVTreeT::iterator it = bindsCombined->begin();
                                                    it!= bindsCombined->end();
                                                    it++)
                if (STLhas(*bindsInTarget, it->first))
                    (*BoundUniversal->bindings)[it->first] = it->second;*/

            /// Now:
            /// I changed the unifiesTo function so that lhs/rhs binding distinction was blurred.
            /// TODO: Verify that this is correct: now including bindings on both sides!

            BoundUniversal->bindings = bindsCombined;


            cprintf(4,"\nresult binds:");

            for_each(BoundUniversal->bindings->begin(), BoundUniversal->bindings->end(), pr2);

            ret->insert(BoundUniversal);
        }
    }

    return ret;
}
#endif

} // namespace reasoning

